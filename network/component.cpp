#include "stdafx.h"
#include "component.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace config
{
    const auto net_thread_count = boost::thread::hardware_concurrency() / 4;
}
namespace detail
{
    using protocal_type = net::protocal::http;
    using request_body = boost::beast::http::empty_body;
    using response_body = boost::beast::http::dynamic_body;
    using recv_buffer = response_body::value_type;
    using response_container = net::protocal::http::protocal_base::response_type<response_body>;
    using net_session_ptr = net::client::session_ptr<protocal_type>;
    using ordinal = std::pair<int16_t, int16_t>;
    using io_context_ptr = std::invoke_result_t<decltype(&net::create_running_asio_pool), unsigned>;
}
namespace net::error
{
    struct http_bad_request : std::runtime_error
    {
        using runtime_error::runtime_error;
        using runtime_error::operator=;
    };
}
namespace net::component
{
    const auto logger = spdlog::stdout_color_mt("net.component.dash.manager");

    //-- dash_manager
    struct dash_manager::impl
    {
        detail::io_context_ptr io_context;
        detail::net_session_ptr net_client;
        std::optional<folly::Uri> mpd_uri;
        std::optional<protocal::dash::parser> mpd_parser;
        std::optional<client::connector<protocal::tcp>> connector;
        folly::Function<double(ordinal)> predictor = [](auto&&) { return 1; };
        frame_consumer_builder consumer_builder;

        struct deleter : std::default_delete<impl>
        {
            void operator()(impl* impl) noexcept {
                impl->io_context = nullptr;
                static_cast<default_delete&>(*this)(impl);
            }
        };

    };

    dash_manager::dash_manager(std::string&& mpd_url, unsigned concurrency)
        : impl_(new impl{}, impl::deleter{}) {
        impl_->io_context = net::create_running_asio_pool(concurrency);
        impl_->mpd_uri.emplace(mpd_url);
        impl_->connector.emplace(*impl_->io_context);
    }

    boost::future<dash_manager> dash_manager::async_create_parsed(std::string mpd_url) {
        static_assert(std::is_move_assignable<dash_manager>::value);
        logger->info("async_create_parsed @{} chain start", boost::this_thread::get_id());
        dash_manager dash_manager{ std::move(mpd_url) };
        return boost::async(
            [dash_manager] {
                logger->info("async_create_parsed @{} establish session", boost::this_thread::get_id());
                auto session_ptr = dash_manager.impl_->connector
                    ->establish_session<detail::protocal_type>(
                        dash_manager.impl_->mpd_uri->host(), std::to_string(dash_manager.impl_->mpd_uri->port()));
                return session_ptr.get();
            }
        ).then(
            [dash_manager](boost::future<detail::net_session_ptr> future_session) {
                logger->info("async_create_parsed @{} send request", boost::this_thread::get_id());
                dash_manager.impl_->net_client = future_session.get();
                return dash_manager.impl_->net_client
                    ->async_send_request(net::make_http_request<detail::request_body>(
                        dash_manager.impl_->mpd_uri->host(), dash_manager.impl_->mpd_uri->path()));
            }
        ).unwrap().then(
            [dash_manager](boost::future<detail::response_container> future_response) {
                logger->info("async_create_parsed @{} parse mpd", boost::this_thread::get_id());
                auto response = future_response.get();
                if (response.result() != boost::beast::http::status::ok)
                    core::throw_with_stacktrace(error::http_bad_request{ __FUNCSIG__ });
                auto mpd_content = boost::beast::buffers_to_string(response.body().data());
                dash_manager.impl_->mpd_parser.emplace(mpd_content);
                return dash_manager;
            });
    }

    std::pair<int16_t, int16_t> dash_manager::scale_size() const {
        return impl_->mpd_parser->scale_size();
    }

    std::pair<int16_t, int16_t> dash_manager::grid_size() const {
        return impl_->mpd_parser->grid_size();
    }

    folly::Function<multi_buffer()>
        dash_manager::tile_supplier(
            int row, int column, folly::Function<double()> predictor) {
        auto& video_set = impl_->mpd_parser->video_set(column, row);
        auto represent_predictor =
            [&video_set, predictor = std::move(predictor)]()->decltype(auto) {
            auto& video_represents = video_set.represents;
            const auto predict_index = boost::numeric_cast<size_t>(
                video_represents.size()*std::invoke(core::as_mutable(predictor)));
            return video_represents.at(std::max(predict_index, video_represents.size() - 1));
        };
        return[&video_set, represent_predictor = std::move(represent_predictor)]{
            multi_buffer x;
            return multi_buffer{};
        };
    }

    void dash_manager::register_represent_consumer(frame_consumer_builder builder) const {
        impl_->consumer_builder = std::move(builder);
    }

    void dash_manager::wait_all_tile_consumed() {
        for (auto& video_set : impl_->mpd_parser->video_set()) {
            try
            {
              
            }
            catch (...)
            {
                
            }
        }
    }

    folly::Function<size_t(dash_manager::ordinal)>
        dash_manager::represent_indexer(folly::Function<double()> probability) {
        return[this, probability = std::move(probability)](ordinal ordinal) mutable {
            const auto[x, y] = ordinal;
            auto& video_set = impl_->mpd_parser->video_set(x, y);
            const auto represent_size = video_set.represents.size();
            const auto predict_index = boost::numeric_cast<size_t>(represent_size*std::invoke(probability));
            return std::max(predict_index, represent_size - 1);
        };
    }
}
