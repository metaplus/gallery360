#include "stdafx.h"
#include "dash.manager.h"
#include "dash.protocal.h"
#include "connector.h"
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/logic/tribool.hpp>
#include <fmt/ostream.h>
#include <folly/futures/FutureSplitter.h>
#include <folly/Random.h>
#include <folly/Uri.h>

using boost::logic::tribool;
using boost::logic::indeterminate;
using net::protocal::http;
using net::protocal::dash;
using ordinal = std::pair<int16_t, int16_t>;
using http_session_ptr = net::client::session<http>::pointer;
using io_context_ptr = std::invoke_result_t<decltype(&net::make_asio_pool), unsigned>;
using net::dash_manager;

namespace net::protocal
{
    struct dash::video_adaptation_set::context final
    {
        boost::circular_buffer<size_t> trace{ 120 };
        size_t trace_index = 0;
        folly::SemiFuture<http_session_ptr> http_session = folly::SemiFuture<http_session_ptr>::makeEmpty();
        bool drain = false;
    };
}

namespace debug
{
    constexpr auto enable_trace = false;
}

namespace net
{
    //-- buffer_sequence
    buffer_sequence::buffer_sequence(detail::multi_buffer& initial,
                                     detail::multi_buffer&& data)
        : initial(initial)
        , data(std::move(data)) {}

    buffer_sequence::buffer_sequence(buffer_sequence&& that) noexcept
        : initial(that.initial)
        , data(std::move(that.data)) {}
}

namespace net
{
    //-- dash_manager
    struct dash_manager::impl final
    {
        io_context_ptr io_context;
        http_session_ptr manager_client;
        std::optional<folly::Uri> mpd_uri;
        std::optional<dash::parser> mpd_parser;
        std::optional<client::connector<protocal::tcp>> connector;
        spdlog::sink_ptr logger_sink;
        std::shared_ptr<spdlog::logger> logger;
        std::shared_ptr<folly::ThreadPoolExecutor> executor;
        int drain_count = 0;
        std::variant<detail::predict_callback,
                     detail::select_callback> adaptation_callback{
            std::in_place_type<detail::predict_callback>,
            [](int, int) {
                return folly::Random::randDouble01();
            }
        };

        struct deleter final : std::default_delete<impl>
        {
            void operator()(impl* impl) noexcept {
                static_cast<default_delete&>(*this)(impl);
            }
        };

        void mark_drained(dash::video_adaptation_set& video_set) {
            if (!std::exchange(video_set.context->drain, true)) { }
            drain_count++;
        }

        dash::represent& predict_represent(dash::video_adaptation_set& video_set) {
            size_t represent_index = 0;
            if (auto* predict = std::get_if<detail::predict_callback>(&adaptation_callback); predict != nullptr) {
                const auto predict_index = [=, &video_set]() {
                    const auto represent_size = std::size(video_set.represents);
                    const auto probability = std::invoke(*predict, video_set.col, video_set.row);
                    const auto predict_index = represent_size * (1 - probability);
                    return std::min(static_cast<size_t>(predict_index), represent_size - 1);
                };
                represent_index = predict_index();
            } else {
                auto* select = std::get_if<detail::select_callback>(&adaptation_callback);
                const auto select_qp = std::invoke(*select, video_set.col, video_set.row);
                const auto iterator = std::find_if(
                    video_set.represents.begin(), video_set.represents.end(),
                    [select_qp](const dash::represent& represent) {
                        assert(represent.qp != 0);
                        return represent.qp == select_qp;
                    });
                represent_index = folly::to<int>(
                    std::distance(video_set.represents.begin(), iterator));
            }
            auto& represent = video_set.represents.at(represent_index);
            video_set.context->trace.push_back(represent_index);
            video_set.context->trace_index++;
            return represent;
        }

        static std::string concat_url_suffix(dash::video_adaptation_set& video_set,
                                             dash::represent& represent) {
            return fmt::format(represent.media, video_set.context->trace_index);
        }

        folly::SemiFuture<multi_buffer>
        request_send(dash::video_adaptation_set& video_set,
                     dash::represent& represent, bool initial = false) {
            const auto replace_suffix = [](std::string path,
                                           std::string&& suffix) {
                if (path.empty()) {
                    return path.assign(std::move(suffix));
                }
                assert(path.front() == '/');
                return path.replace(path.rfind('/') + 1, path.size(), suffix);
            };
            const auto suffix = [&video_set, &represent](bool initial) {
                return initial
                           ? represent.initial
                           : fmt::format(represent.media, video_set.context->trace_index);
            };
            const auto url_path = replace_suffix(mpd_uri->path(), suffix(initial));
            return video_set.context
                            ->http_session.wait().value()
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
                                  .via(executor.get()).thenValue(
                                      [](multi_buffer&& initial_buffer) {
                                          return std::make_shared<multi_buffer>(std::move(initial_buffer));
                                      }));
            }
            return represent.initial_buffer
                            ->getSemiFuture();
        }

        folly::SemiFuture<http_session_ptr> make_http_session() {
            return connector->establish_session<http>(mpd_uri->host(),
                                                      folly::to<std::string>(mpd_uri->port()))
                            .deferValue([this](http_session_ptr session) {
                                session->trace_by(logger_sink);
                                return session;
                            });
        }
    };

    dash_manager::dash_manager(std::string mpd_url, unsigned concurrency,
                               std::shared_ptr<folly::ThreadPoolExecutor> executor)
        : impl_(new impl{}, impl::deleter{}) {
        impl_->io_context = net::make_asio_pool(concurrency);
        impl_->mpd_uri.emplace(mpd_url);
        impl_->connector.emplace(*impl_->io_context);
        impl_->executor = std::move(executor);
    }

    folly::Future<dash_manager> dash_manager::request_stream_index() const {
        auto self = std::move(*this);
        return impl_->make_http_session()
                    .via(impl_->executor.get()).thenMulti(
                        [self](http_session_ptr session) mutable {
                            return (self.impl_->manager_client = std::move(session))
                                ->send_request_for<multi_buffer>(
                                    net::make_http_request<empty_body>(self.impl_->mpd_uri->host(),
                                                                       self.impl_->mpd_uri->path()));
                        },
                        [self](multi_buffer&& buffer) {
                            auto mpd_content = buffers_to_string(buffer.data());
                            self.impl_->mpd_parser
                                .emplace(std::move(mpd_content), self.impl_->executor);
                            return self;
                        });
    }

    core::dimension dash_manager::frame_size() const {
        return impl_->mpd_parser
                    ->scale();
    }

    core::dimension dash_manager::tile_size() const {
        const auto frame_size = this->frame_size();
        const auto grid_size = this->grid_size();
        return {
            frame_size.width / grid_size.col,
            frame_size.height / grid_size.row
        };
    }

    core::coordinate dash_manager::grid_size() const {
        return impl_->mpd_parser
                    ->grid();
    }

    int dash_manager::tile_count() const {
        const auto grid_size = this->grid_size();
        return grid_size.col * grid_size.row;
    }

    bool dash_manager::available() const {
        return impl_->drain_count == 0;
    }

    void dash_manager::trace_by(spdlog::sink_ptr sink) const {
        impl_->logger_sink = sink;
        impl_->logger = core::make_async_logger("dash.manager", sink);
    }

    void dash_manager::predict_by(detail::predict_callback callback) const {
        impl_->adaptation_callback
             .emplace<detail::predict_callback>(std::move(callback));
    }

    void dash_manager::select_by(detail::select_callback callback) const {
        impl_->adaptation_callback
             .emplace<detail::select_callback>(std::move(callback));
    }

    folly::Function<folly::SemiFuture<buffer_sequence>()>
    dash_manager::tile_streamer(core::coordinate coordinate) {
        auto& video_set = impl_->mpd_parser->video_set(coordinate);
        assert(video_set.col == coordinate.col);
        assert(video_set.row == coordinate.row);
        core::access(video_set.context)->http_session = impl_->make_http_session();
        return [this, &video_set] {
            if (video_set.context->drain) {
                return folly::makeSemiFuture<buffer_sequence>(core::stream_drained_error{});
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
                        return buffer_sequence{ **initial_buffer, std::move(*data_buffer) };
                    }
                );
        };
    }
}
