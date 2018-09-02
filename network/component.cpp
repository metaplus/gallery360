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
    using net_session_ptr = net::client::session_ptr<protocal_type, net::policy<response_body>>;
    using ordinal = std::pair<int16_t, int16_t>;

    const auto logger = spdlog::stdout_color_mt("net.component");
    boost::thread_group net_threads;

    auto create_running_asio_pool() {
        struct asio_deleter : std::default_delete<boost::asio::io_context>
        {
            using resource = boost::asio::io_context;
            using guardian = boost::asio::executor_work_guard<resource::executor_type>;
            std::unique_ptr<guardian> guard;
            std::vector<boost::thread*> threads;

            asio_deleter() = default;
            asio_deleter(asio_deleter const&) = delete;
            asio_deleter(asio_deleter&&) noexcept = default;
            asio_deleter& operator=(asio_deleter const&) = delete;
            asio_deleter& operator=(asio_deleter&&) noexcept = default;
            ~asio_deleter() = default;

            explicit asio_deleter(resource* io_context)
                : guard(std::make_unique<guardian>(boost::asio::make_work_guard(*io_context))) {
                std::generate_n(
                    std::back_inserter(threads),
                    config::net_thread_count,
                    [this, io_context] {
                        return net_threads.create_thread(
                            [this, io_context] {
                                const auto thread_id = boost::this_thread::get_id();
                                try {
                                    logger->info("asio thread {} start", thread_id);
                                    io_context->run();
                                    logger->info("asio thread {} finish", thread_id);
                                } catch (...) {
                                    const auto message = boost::current_exception_diagnostic_information();
                                    logger->error("asio thread {} error {}", thread_id, message);
                                }
                                logger->info("asio thread {} exit", thread_id);
                            });
                    });
            }
            void operator()(resource* io_context) {
                guard->reset();
                auto join_count = 0;
                for (auto* const thread : threads) {
                    if (thread->joinable()) {
                        thread->join();
                        ++join_count;
                    }
                }
                logger->info("asio thread_group join count {}", join_count);
                static_cast<default_delete&>(*this)(io_context);
                logger->info("asio io_context destructed");
            }
        };
        auto* io_context = new boost::asio::io_context();
        return std::unique_ptr<boost::asio::io_context, asio_deleter>{
            io_context, asio_deleter{ io_context }
        };
    }
    using io_context_ptr = std::invoke_result_t<decltype(create_running_asio_pool)>;
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
    //-- dash_stream_context
    struct dash_manager::impl
    {
        detail::io_context_ptr io_context;
        detail::net_session_ptr net_client;
        std::optional<folly::Uri> mpd_uri;
        std::optional<protocal::dash::parser> mpd_parser;
        std::optional<client::connector<protocal::tcp>> connector;
        struct pending_task
        {
            boost::future<detail::net_session_ptr> future_session_ptr;
        } pending;
        struct deleter : std::default_delete<impl>
        {
            void operator()(impl* impl) noexcept {
                impl->io_context = nullptr;
                static_cast<default_delete&>(*this)(impl);
            }
        };
    };
    dash_manager::dash_manager(std::string&& mpd_url)
        : impl_(new impl{}, impl::deleter{}) {
        impl_->io_context = detail::create_running_asio_pool();
        impl_->mpd_uri.emplace(mpd_url);
        impl_->connector.emplace(*impl_->io_context);
        impl_->pending.future_session_ptr = impl_->connector
            ->establish_session<detail::protocal_type, detail::response_body>(
                impl_->mpd_uri->host(), std::to_string(impl_->mpd_uri->port()));
    }
    boost::future<dash_manager> dash_manager::async_create_parsed(std::string mpd_url) {
        static_assert(std::is_move_assignable<dash_manager>::value);
        dash_manager dash_manager{ std::move(mpd_url) };
        return dash_manager.impl_->pending.future_session_ptr
            .then(
                boost::launch::sync,
                [dash_manager](boost::future<detail::net_session_ptr> future_session) {
                    dash_manager.impl_->net_client = future_session.get();
                    return dash_manager.impl_->net_client
                        ->async_send_request(net::make_http_request<detail::request_body>(
                            dash_manager.impl_->mpd_uri->host(), dash_manager.impl_->mpd_uri->path()));
                })
            .unwrap().then(
                boost::launch::sync,
                [dash_manager](boost::future<detail::response_container> future_response) {
                    auto response = future_response.get();
                    if (response.result() != boost::beast::http::status::ok)
                        core::throw_with_stacktrace(error::http_bad_request{ __PRETTY_FUNCTION__ });
                    auto mpd_content = boost::beast::buffers_to_string(response.body().data());
                    dash_manager.impl_->mpd_parser.emplace(mpd_content);
                    return dash_manager;
                });
    }
}
