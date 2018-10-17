#include "stdafx.h"
#include "component.h"
#include <boost/circular_buffer.hpp>
#include <boost/logic/tribool.hpp>
#include <folly/futures/FutureSplitter.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

using boost::logic::tribool;
using boost::logic::indeterminate;
using net::protocal::http;
using net::protocal::dash;
using request_body = boost::beast::http::empty_body;
using response_body = boost::beast::http::dynamic_body;
using recv_buffer = response_body::value_type;
using response = http::protocal_base::response<response_body>;
using net::client::http_session_ptr;
using ordinal = std::pair<int16_t, int16_t>;
using io_context_ptr = std::invoke_result_t<decltype(&net::create_running_asio_pool), unsigned>;
using net::component::dash_manager;
using net::component::frame_consumer;
using net::component::frame_builder;
using net::component::frame_indexed_builder;

namespace net::protocal
{
    struct dash::video_adaptation_set::context
    {
        std::vector<size_t> trace;
        boost::circular_buffer<folly::Future<frame_consumer>> consumer_cycle{ 3 };
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
    buffer_context::buffer_context(detail::multi_buffer& initial, detail::multi_buffer&& data)
        : initial(initial)
        , data(std::move(data)) {}

    buffer_context::buffer_context(buffer_context&& that) noexcept
        : initial(that.initial)
        , data(std::move(data)) {}
}

namespace net::component
{
    const auto logger = spdlog::stdout_color_mt("net.component.dash.manager");

    //-- dash_manager
    struct dash_manager::impl
    {
        io_context_ptr io_context;
        http_session_ptr net_client;
        std::optional<folly::Uri> mpd_uri;
        std::optional<dash::parser> mpd_parser;
        std::optional<client::connector<protocal::tcp>> connector;
        folly::Function<double(int, int)> predictor = [](auto, auto) { return 1; };
        folly::Function<size_t(int, int)> indexer;
        folly::ThreadPoolExecutor& executor = dynamic_cast<folly::ThreadPoolExecutor&>(*folly::getCPUExecutor());
        frame_indexed_builder consumer_builder;
        int drain_count = 0;

        struct deleter : std::default_delete<impl>
        {
            void operator()(impl* impl) noexcept {
                impl->io_context = nullptr;
                static_cast<default_delete&>(*this)(impl);
            }
        };

        void mark_drained(dash::video_adaptation_set& video_set) {
            if (!std::exchange(video_set.context->drain, true)) {
                logger->warn("video_set drained[x:{}, y:{}]", video_set.x, video_set.y);
            }
            drain_count++;
        }

        dash::represent& predict_represent(dash::video_adaptation_set& video_set) {
            const auto predict_index = [this, &video_set]() {
                const auto represent_size = std::size(video_set.represents);
                return std::min(boost::numeric_cast<size_t>(
                    represent_size*std::invoke(predictor, video_set.x, video_set.y)),
                    represent_size - 1);
            };
            const auto represent_index = predict_index();
            auto& represent = video_set.represents.at(represent_index);
            video_set.context->trace.push_back(represent_index);
            return represent;
        }

        std::string concat_url_suffix(dash::video_adaptation_set& video_set, dash::represent& represent) const {
            return fmt::format(represent.media, std::size(video_set.context->trace));
        }

        /*folly::Future<frame_consumer> request_tile_consumer(dash::video_adaptation_set& video_set) {
            auto& context = *video_set.context;
            if (!video_set.context) {
                core::access(video_set.context)->trace.reserve(1024);
            }
            if (context.drain) {
                return folly::makeFuture<frame_consumer>(
                    core::bad_request_error{ "dummy placeholder" });
            }
            auto& represent = predict_represent(video_set);
            if (!represent.initial_buffer.valid()) {
                represent.initial_buffer = request_initial(represent);
            }
            return request_send(concat_url_suffix(video_set, represent))
                .via(&executor).thenValue(
                    [this, &video_set, &represent](multi_buffer&& tile_buffer) {
                        represent.initial_buffer.wait();
                        return consumer_builder(std::make_pair(video_set.x, video_set.y),
                                                represent.initial_buffer.value(),
                                                std::move(tile_buffer));
                    });
        }*/

        folly::SemiFuture<multi_buffer> request_send(std::string suffix) {
            assert(!suffix.empty() && suffix.front() != '/');
            const auto replace_suffix = [](std::string path, std::string& suffix) {
                if (path.empty()) {
                    return path.assign(std::move(suffix));
                }
                assert(path.front() == '/');
                return path.replace(path.rfind('/') + 1, path.size(), suffix);
            };
            const auto url_path = replace_suffix(mpd_uri->path(), suffix);
            return net_client->async_send_request_for<multi_buffer>(
                net::make_http_request<empty_body>(mpd_uri->host(), url_path));
        }

        folly::SemiFuture<std::shared_ptr<multi_buffer>> request_initial_if_null(dash::represent& represent) {
            if (!represent.initial_buffer) {
                represent.initial_buffer.emplace(
                    request_send(represent.initial).via(&executor).thenValue(
                        [](multi_buffer&& initial_buffer) {
                            return std::make_shared<multi_buffer>(std::move(initial_buffer));
                        }));
            }
            return represent.initial_buffer->getSemiFuture();
        }

        enum consume_result
        {
            fail = false,
            success = true,
            exception,
            pending,
        };

        consume_result consume_tile(dash::video_adaptation_set& video_set,
                                    bool poll,
                                    bool reset = false) {
            auto& context = *video_set.context;
            try {
                assert(context.consumer_cycle.full());
                if (context.drain) {
                    core::throw_drained("consume_tile");
                }
                auto& consumer = context.consumer_cycle.front();
                if (poll) {
                    if (!consumer.isReady()) {
                        return pending;
                    }
                } else {
                    consumer.wait();
                }
                if (!consumer.value()()) {
                    assert(!reset);
                    context.consumer_cycle.pop_front();
                    //context.consumer_cycle.push_back(request_tile_consumer(video_set));
                    return consume_tile(video_set, poll, true);
                }
                return success;
            } catch (...) {
                mark_drained(video_set);
                return exception;
            }
        }

        bool consume_until_complete(dash::video_adaptation_set& video_set,
                                    folly::QueuedImmediateExecutor& executor,
                                    int& exception_count,
                                    bool poll = true) {

            auto& context = *video_set.context;
            auto try_consume = [this, &context](bool poll) -> consume_result {
                assert(context.consumer_cycle.full());
                if (context.drain) {
                    core::throw_drained("context.drain");
                }
                auto& consumer_front = context.consumer_cycle.front();
                if (poll) {
                    if (!consumer_front.isReady()) {
                        return pending;
                    }
                } else {
                    consumer_front.wait();
                }
                return consumer_front.hasValue() ?
                    static_cast<consume_result>(consumer_front.value()()) : exception;
            };
            auto next_poll = false;
            switch (try_consume(poll)) {
            case fail:
                context.consumer_cycle.pop_front();
                //context.consumer_cycle.push_back(request_tile_consumer(video_set));
                next_poll = true;
            case pending:
                executor.add(
                    [this, next_poll, &video_set, &executor, &exception_count] {
                        consume_until_complete(video_set, executor, exception_count, next_poll);
                    });
                break;
            case exception:
                mark_drained(video_set);
                exception_count++;
                break;
            case success:
            default:
                return true;
            }
            return false;
        }
    };

    dash_manager::dash_manager(std::string&& mpd_url, unsigned concurrency)
        : impl_(new impl{}, impl::deleter{}) {
        impl_->io_context = net::create_running_asio_pool(concurrency);
        impl_->mpd_uri.emplace(mpd_url);
        impl_->connector.emplace(*impl_->io_context);
    }

#ifdef BIGOBJ_LIMIT
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
            [dash_manager](boost::future<detail::http_session_ptr> future_session) {
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
#endif

    folly::Future<dash_manager> dash_manager::create_parsed(std::string mpd_url, unsigned concurrency) {
        static_assert(std::is_move_assignable<dash_manager>::value);
        auto& executor = *folly::getCPUExecutor();
        logger->info("async_create_parsed @{} chain start", folly::getCurrentThreadID());
        dash_manager dash_manager{ std::move(mpd_url),concurrency };
        logger->info("async_create_parsed @{} establish session", boost::this_thread::get_id());
        return dash_manager.impl_->connector
            ->establish_session<http>(
                dash_manager.impl_->mpd_uri->host(),
                std::to_string(dash_manager.impl_->mpd_uri->port()),
                core::folly)
            .via(&executor).thenValue(
                [dash_manager](http_session_ptr session) {
                    logger->info("async_create_parsed @{} send request", boost::this_thread::get_id());
                    dash_manager.impl_->net_client = std::move(session);
                    return dash_manager.impl_->net_client
                        ->async_send_request_for<multi_buffer>(
                            net::make_http_request<empty_body>(
                                dash_manager.impl_->mpd_uri->host(),
                                dash_manager.impl_->mpd_uri->path()));
                })
            .via(&executor).thenValue(
                [dash_manager](multi_buffer buffer) {
                    logger->info("async_create_parsed @{} parse mpd", boost::this_thread::get_id());
                    auto mpd_content = boost::beast::buffers_to_string(buffer.data());
                    dash_manager.impl_->mpd_parser.emplace(std::move(mpd_content));
                    return dash_manager;
                });
    }

    std::pair<int, int> dash_manager::scale_size() const {
        return impl_->mpd_parser->scale_size();
    }

    std::pair<int, int> dash_manager::grid_size() const {
        return impl_->mpd_parser->grid_size();
    }

    void dash_manager::register_represent_builder(frame_indexed_builder builder) const {
        throw core::not_implemented_error{ __FUNCTION__ };
        //assert(!impl_->consumer_builder);
        //impl_->consumer_builder = std::move(builder);
        //auto iteration = 0;
        //auto loop = true;
        //while (loop) {
        //    for (auto& video_set : impl_->mpd_parser->video_set()) {
        //        auto& consumer_cycle = core::access(video_set.context)->consumer_cycle;
        //        if (consumer_cycle.full()) {
        //            loop = false;
        //            break;
        //        }
        //        consumer_cycle.push_back(impl_->request_tile_consumer(video_set));
        //        iteration++;
        //    }
        //}
        //logger->info("register_represent_builder[iteration:{}]", iteration);
    }

    bool dash_manager::available() const {
        return impl_->drain_count == 0;
    }

    bool dash_manager::poll_tile_consumed(int col, int row) const {
        auto& video_set = impl_->mpd_parser->video_set(col, row);
        const auto result = impl_->consume_tile(video_set, true);
        assert(result == impl::success || result == impl::pending || result == impl::exception);
        return result == impl::success;
    }

    bool dash_manager::wait_tile_consumed(int col, int row) const {
        auto& video_set = impl_->mpd_parser->video_set(col, row);
        const auto result = impl_->consume_tile(video_set, false);
        assert(result == impl::success || result == impl::exception);
        return result == impl::success;
    }

    int dash_manager::wait_full_frame_consumed() {
        auto poll_count = 0;
        auto exception_count = 0;
        folly::QueuedImmediateExecutor executor;
        executor.add(
            [this, &executor, &poll_count, &exception_count] {
                for (auto& video_set : impl_->mpd_parser->video_set()) {
                    poll_count += impl_->consume_until_complete(video_set, executor, exception_count, true);
                }
            });
        return exception_count;
    }

    folly::SemiFuture<buffer_context>
        dash_manager::request_tile_context(int col, int row) const {
        auto& video_set = impl_->mpd_parser->video_set(col, row);
        if (!video_set.context) {
            core::access(video_set.context)->trace.reserve(1024);
        }
        if (video_set.context->drain) {
            return folly::makeSemiFuture<buffer_context>(core::stream_drained_error{ __FUNCTION__ });
        }
        auto& represent = impl_->predict_represent(video_set);
        return folly::collectAllSemiFuture(
            impl_->request_initial_if_null(represent),
            impl_->request_send(impl_->concat_url_suffix(video_set, represent))
        ).deferValue(
            [](std::tuple<folly::Try<std::shared_ptr<multi_buffer>>, folly::Try<multi_buffer>>&& buffer_tuple) {
                auto&[initial_buffer, data_buffer] = buffer_tuple;
                data_buffer.throwIfFailed();
                return buffer_context{ **initial_buffer,std::move(*data_buffer) };
            });
    }

    folly::Function<size_t(int, int)>
        dash_manager::represent_indexer(folly::Function<double(int, int)> probability) {
        return[this, probability = std::move(probability)](int x, int y) mutable {
            auto& video_set = impl_->mpd_parser->video_set(x, y);
            const auto represent_size = video_set.represents.size();
            const auto predict_index = boost::numeric_cast<size_t>(represent_size*probability(x, y));
            return std::max(predict_index, represent_size - 1);
        };
    }
}
