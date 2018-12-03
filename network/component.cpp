#include "stdafx.h"
#include "component.h"
#include <boost/circular_buffer.hpp>
#include <boost/logic/tribool.hpp>
#include <folly/futures/FutureSplitter.h>

using boost::logic::tribool;
using boost::logic::indeterminate;
using net::protocal::http;
using net::protocal::dash;
using net::client::http_session_ptr;
using ordinal = std::pair<int16_t, int16_t>;
using io_context_ptr = std::invoke_result_t<decltype(&net::make_asio_pool), unsigned>;
using net::component::dash_manager;
using net::component::frame_consumer;
using net::component::frame_indexed_builder;

namespace net::protocal
{
    struct dash::video_adaptation_set::context
    {
        std::vector<size_t> trace;
        folly::SemiFuture<http_session_ptr> tile_client = folly::SemiFuture<http_session_ptr>::makeEmpty();
        bool drain = false;
    };
}

namespace debug
{
    constexpr auto enable_trace = false;
}

namespace net
{
    //-- buffer_context
    buffer_context::buffer_context(detail::multi_buffer& initial,
                                   detail::multi_buffer&& data)
        : initial(initial)
        , data(std::move(data)) {}

    buffer_context::buffer_context(buffer_context&& that) noexcept
        : initial(that.initial)
        , data(std::move(that.data)) {}
}

namespace net::component
{
    const auto logger = spdlog::stdout_color_mt("net.dash.manager");

    //-- dash_manager
    struct dash_manager::impl
    {
        io_context_ptr io_context;
        http_session_ptr manager_client;
        std::optional<folly::Uri> mpd_uri;
        std::optional<dash::parser> mpd_parser;
        std::optional<client::connector<protocal::tcp>> connector;
        folly::Function<double(int, int)> predictor = [](auto, auto) {
            return folly::Random::randDouble01();
        };
        folly::Function<size_t(int, int)> indexer;
        folly::ThreadPoolExecutor& executor = dynamic_cast<folly::ThreadPoolExecutor&>(*folly::getCPUExecutor());
        frame_indexed_builder consumer_builder;
        int drain_count = 0;
        detail::trace_callback trace_callback;

        struct deleter final : std::default_delete<impl>
        {
            void operator()(impl* impl) noexcept {
                logger->info("destructor deleting io_context");
                static_cast<default_delete&>(*this)(impl);
            }
        };

        void mark_drained(dash::video_adaptation_set& video_set) {
            if (!std::exchange(video_set.context->drain, true)) {
                logger->warn("video_set drained[col{} row{}]", video_set.col, video_set.row);
            }
            drain_count++;
        }

        dash::represent& predict_represent(dash::video_adaptation_set& video_set) {
            const auto predict_index = [this, &video_set]() {
                const auto represent_size = std::size(video_set.represents);
                const auto probability = std::invoke(predictor, video_set.col, video_set.row);;
                const auto predict_index = represent_size * probability;
                return std::min(static_cast<size_t>(predict_index),
                                represent_size - 1);
            };
            const auto represent_index = predict_index();
            auto& represent = video_set.represents.at(represent_index);
            video_set.context->trace.push_back(represent_index);
            return represent;
        }

        std::string concat_url_suffix(dash::video_adaptation_set& video_set,
                                      dash::represent& represent) const {
            return fmt::format(represent.media,
                               std::size(video_set.context->trace));
        }

        folly::SemiFuture<multi_buffer>
        request_send(dash::video_adaptation_set& video_set,
                     dash::represent& represent,
                     bool initial = false) {
            const auto replace_suffix = [](std::string path,
                                           std::string&& suffix) {
                if (path.empty()) {
                    return path.assign(std::move(suffix));
                }
                assert(path.front() == '/');
                return path.replace(path.rfind('/') + 1,
                                    path.size(),
                                    suffix);
            };
            const auto suffix = [&video_set, &represent](bool initial) {
                return initial
                           ? represent.initial
                           : fmt::format(represent.media, std::size(video_set.context->trace));
            };
            const auto url_path = replace_suffix(mpd_uri->path(), suffix(initial));
            return video_set.context
                            ->tile_client.wait().value()
                            ->send_request_for<multi_buffer>(
                                net::make_http_request<empty_body>(mpd_uri->host(),
                                                                   url_path));
        }

        folly::SemiFuture<std::shared_ptr<multi_buffer>>
        request_initial_if_null(dash::video_adaptation_set& video_set,
                                dash::represent& represent) {
            if (!represent.initial_buffer) {
                represent.initial_buffer
                         .emplace(request_send(video_set, represent, true)
                                  .via(&executor).thenValue(
                                      [](multi_buffer&& initial_buffer) {
                                          return std::make_shared<multi_buffer>(std::move(initial_buffer));
                                      }));
            }
            return represent.initial_buffer
                            ->getSemiFuture();
        }

        folly::SemiFuture<http_session_ptr> make_http_client() {
            return connector->establish_session<http>(mpd_uri->host(),
                                                      folly::to<std::string>(mpd_uri->port()))
                            .deferValue([this](http_session_ptr session) {
                                if (trace_callback) {
                                    session->trace_by(trace_callback);
                                }
                                return session;
                            });
        }
    };

    dash_manager::dash_manager(std::string&& mpd_url,
                               unsigned concurrency)
        : impl_(new impl{}, impl::deleter{}) {
        impl_->io_context = net::make_asio_pool(concurrency);
        impl_->mpd_uri.emplace(mpd_url);
        impl_->connector.emplace(*impl_->io_context);
    }

    folly::Future<dash_manager> dash_manager::create_parsed(std::string mpd_url,
                                                            unsigned concurrency,
                                                            detail::trace_callback callback) {
        static_assert(std::is_move_assignable<dash_manager>::value);
        auto executor = folly::getCPUExecutor();
        logger->info("create_parsed @{} chain start", folly::getCurrentThreadID());
        dash_manager dash_manager{ std::move(mpd_url), concurrency };
        dash_manager.impl_->trace_callback = std::move(callback);
        logger->info("create_parsed @{} establish session", std::this_thread::get_id());
        return dash_manager.impl_
                           ->make_http_client()
                           .via(executor.get()).thenValue(
                               [dash_manager](http_session_ptr session) mutable {
                                   logger->info("create_parsed @{} send request", std::this_thread::get_id());;
                                   return (dash_manager.impl_->manager_client = std::move(session))
                                       ->send_request_for<multi_buffer>(
                                           net::make_http_request<empty_body>(dash_manager.impl_->mpd_uri->host(),
                                                                              dash_manager.impl_->mpd_uri->path()));
                               })
                           .via(executor.get()).thenValue(
                               [dash_manager](multi_buffer&& buffer) {
                                   logger->info("create_parsed @{} parse mpd", std::this_thread::get_id());
                                   auto mpd_content = buffers_to_string(buffer.data());
                                   dash_manager.impl_
                                               ->mpd_parser
                                               .emplace(std::move(mpd_content));
                                   return dash_manager;
                               });
    }

    std::pair<int, int> dash_manager::scale_size() const {
        return impl_->mpd_parser
                    ->scale_size();
    }

    std::pair<int, int> dash_manager::grid_size() const {
        return impl_->mpd_parser
                    ->grid_size();
    }

    bool dash_manager::available() const {
        return impl_->drain_count == 0;
    }

    folly::SemiFuture<buffer_context>
    dash_manager::request_tile_context(int col, int row) const {
        auto& video_set = impl_->mpd_parser->video_set(col, row);
        assert(video_set.col == col);
        assert(video_set.row == row);
        if (!video_set.context) {
            core::access(video_set.context)->trace.reserve(1024);
        }
        if (video_set.context->drain) {
            return folly::makeSemiFuture<buffer_context>(
                core::stream_drained_error{ __FUNCTION__ });
        }
        if (!video_set.context->tile_client.valid()) {
            video_set.context->tile_client = impl_->make_http_client();
        }
        auto& represent = impl_->predict_represent(video_set);
        auto initial_segment = impl_->request_initial_if_null(video_set, represent);
        auto tile_segment = impl_->request_send(video_set, represent, false);
        return folly::collectAllSemiFuture(initial_segment, tile_segment)
            .deferValue([](
                std::tuple<
                    folly::Try<std::shared_ptr<multi_buffer>>,
                    folly::Try<multi_buffer>
                >&& buffer_tuple) {
                    auto& [initial_buffer, data_buffer] = buffer_tuple;
                    data_buffer.throwIfFailed();
                    return buffer_context{ **initial_buffer, std::move(*data_buffer) };
                }
            );
    }

    folly::Function<size_t(int, int)>
    dash_manager::represent_indexer(folly::Function<double(int, int)> probability) {
        return [this, probability = std::move(probability)](int x, int y) mutable {
            auto& video_set = impl_->mpd_parser->video_set(x, y);
            const auto represent_size = video_set.represents.size();
            const auto predict_index = folly::to<size_t>(represent_size * probability(x, y));
            return std::max(predict_index, represent_size - 1);
        };
    }
}
