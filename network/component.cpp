#include "stdafx.h"
#include "component.h"
#include <boost/circular_buffer.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace net
{
    using protocal::http;
    using protocal::dash;
    using request_body = empty_body;
    using response_body = dynamic_body;
    using recv_buffer = response_body::value_type;
    using response = http::protocal_base::response<response_body>;
    using client::http_session_ptr;
    using ordinal = std::pair<int16_t, int16_t>;
    using io_context_ptr = std::invoke_result_t<decltype(&create_running_asio_pool), unsigned>;
    using component::dash_manager;
    using frame_consumer = dash_manager::frame_consumer;
    using frame_consumer_builder = dash_manager::frame_consumer_builder;
}

namespace net::protocal
{
    struct dash::video_adaptation_set::context
    {
        std::vector<size_t> trace;
        frame_consumer consumer;
        folly::SemiFuture<multi_buffer> initial_consumer = folly::SemiFuture<multi_buffer>::makeEmpty();
        boost::circular_buffer<folly::Future<frame_consumer>> consumer_cycle{ 2 };
        bool drain = false;
    };
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
        folly::Executor& executor = *folly::getCPUExecutor();
        frame_consumer_builder consumer_builder;

        struct deleter : std::default_delete<impl>
        {
            void operator()(impl* impl) noexcept {
                impl->io_context = nullptr;
                static_cast<default_delete&>(*this)(impl);
            }
        };

        size_t predict_represent(dash::video_adaptation_set& video_set) {
            auto represent_size = video_set.represents.size();
            return std::max(boost::numeric_cast<size_t>(
                represent_size*std::invoke(predictor, video_set.x, video_set.y)),
                represent_size - 1);
        }

        folly::Future<frame_consumer> request_tile(dash::video_adaptation_set& video_set) {
            if (video_set.context->drain) {
                return folly::makeFuture<frame_consumer>(
                    core::bad_request_error{ "dummy placeholder" });
            }
            auto represent_index = predict_represent(video_set);
            auto& represent = video_set.represents.at(represent_index);
            auto& context = *core::access(video_set.context);
            context.trace.push_back(represent_index);
            auto path_regex = [](std::string& path, auto index) {
                static const std::regex pattern{ "\\$Number\\$" };
                return std::regex_replace(path, pattern, fmt::to_string(index));
            };
            if (!context.initial_consumer.valid()) {
                context.initial_consumer = request_initial(represent);
            }
            return request_send(path_regex(represent.media, std::size(context.trace)))
                .via(&executor).then(
                    [this, &context](multi_buffer tile_buffer) {
                        context.initial_consumer.wait();
                        return consumer_builder(
                            core::split_buffer_sequence(
                                context.initial_consumer.value(),
                                tile_buffer));
                    });
        }

        folly::SemiFuture<multi_buffer> request_send(std::string path) {
            return net_client
                ->async_send_request_for<multi_buffer>(
                    net::make_http_request<empty_body>(mpd_uri->host(), path));
        }

        folly::SemiFuture<multi_buffer> request_initial(dash::represent& represent) {
            return request_send(represent.initial);
        }

        int consume_tile(dash::video_adaptation_set& video_set) {
            auto& context = *video_set.context;
            auto try_count = 1;
            const auto rotate_async_consumer = [this, &video_set, &context] {
                context.consumer = std::move(context.consumer_cycle.front()).get();
                context.consumer_cycle.pop_front();
                context.consumer_cycle.push_back(request_tile(video_set));
            };
            try {
                assert(context.consumer_cycle.full());
                if (context.drain) {
                    core::throw_drained("consume_tile");
                }
                if (!context.consumer) {
                    rotate_async_consumer();
                }
                while (!context.consumer()) {
                    try_count++;
                    assert(try_count <= 2);
                    context.consumer = nullptr;
                    rotate_async_consumer();
                }
            } catch (...) {
                context.drain = true;
                logger->warn("video_adaptation_set[{},{}] eof", video_set.x, video_set.y);
                return -1;
            }
            return try_count;
        }

        bool consume_until_complete(dash::video_adaptation_set& video_set,
                                    folly::QueuedImmediateExecutor& executor,
                                    int& exception_count, bool poll = true) {
            enum result
            {
                fail = false,
                success = true,
                exception,
                pending,
            };
            auto& context = *video_set.context;
            auto try_consume = [this, &context](bool poll) -> result {
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
                    static_cast<result>(consumer_front.value()()) : exception;
            };
            auto next_poll = false;
            switch (try_consume(poll)) {
            case fail:
                context.consumer_cycle.pop_front();
                context.consumer_cycle.push_back(request_tile(video_set));
                next_poll = true;
            case pending:
                executor.add(
                    [this, &video_set, &executor, &exception_count] {
                        consume_until_complete(video_set, executor, exception_count, false);
                    });
                break;
            case exception:
                if (!std::exchange(context.drain, true)) {
                    logger->warn("video_set drained[x:{}, y:{}]", video_set.x, video_set.y);
                }
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

    folly::Future<dash_manager> dash_manager::async_create_parsed(std::string mpd_url) {
        static_assert(std::is_move_assignable<dash_manager>::value);
        auto& executor = *folly::getCPUExecutor();
        logger->info("async_create_parsed @{} chain start", folly::getCurrentThreadID());
        dash_manager dash_manager{ std::move(mpd_url) };
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

    void dash_manager::register_represent_builder(frame_consumer_builder builder) const {
        impl_->consumer_builder = std::move(builder);
        auto iteration = 0;
        auto loop = true;
        while (loop && ++iteration) {
            for (auto& video_set : impl_->mpd_parser->video_set()) {
                auto& consumer_cycle = video_set.context->consumer_cycle;
                assert(iteration > 1 || consumer_cycle.empty());
                if (consumer_cycle.full()) {
                    loop = false;
                    break;
                }
                consumer_cycle.push_back(impl_->request_tile(video_set));
            }
        }
        logger->info("register_represent_builder[iteration:{}]", iteration);
    }

    bool dash_manager::wait_tile_consumed(int col, int row) {
        auto& video_set = impl_->mpd_parser->video_set(col, row);

        return false;
    }

    int dash_manager::wait_full_frame_consumed() {
        auto poll_count = 0;
        auto exception_count = 0;
        enum result
        {
            fail = false,
            success = true,
            exception,
            pending,
        };
        folly::QueuedImmediateExecutor executor;
        executor.add(
            [this, &executor, &poll_count, &exception_count] {
                for (auto& video_set : impl_->mpd_parser->video_set()) {
                    poll_count += impl_->consume_until_complete(video_set, executor, exception_count, true);
                }
            });
        return exception_count;
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
