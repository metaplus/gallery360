#include "stdafx.h"
#include "component.h"

namespace media::component
{
    using namespace detail;
    using boost::beast::multi_buffer;

    struct frame_segmentor::impl final
    {
        int64_t consume_count = 0;
        multi_buffer init_buffer;
        multi_buffer current_buffer;
        std::optional<io_context> io_context;
        std::optional<format_context> format_context;
        std::optional<codec_context> codec_context;
        std::list<frame> remain_frames;

        frame try_consume_once() {
            frame frame{ nullptr };
            if (remain_frames.size()) {
                frame = std::move(remain_frames.front());
                remain_frames.pop_front();
            } else if (codec_context->valid()) {
                auto packet_empty = false;
                auto frame_size = 0i64;
                do {
                    auto packet = format_context->read(media::type::video);
                    auto frames = codec_context->decode(packet);
                    frame_size = frames.size();
                    packet_empty = packet.empty();
                    if (frame_size) {
                        frame = std::move(frames.front());
                        if (frame_size > 1) {
                            std::move(frames.begin() + 1, frames.end(),
                                      std::back_inserter(remain_frames));
                        }
                    }
                } while (frame_size == 0 && !packet_empty);
            }
            return frame;
        }
    };

    void frame_segmentor::impl_deleter::operator()(impl* impl) {
        static_cast<default_delete&>(*this)(impl);
    }

    frame_segmentor::frame_segmentor(std::list<const_buffer> buffer_list,
                                     unsigned concurrency)
        : impl_{ new impl{}, impl_deleter{} } {
        parse_context(std::move(buffer_list), concurrency);
    }

    frame_segmentor::operator bool() const {
        return impl_.operator bool();
    }

    void frame_segmentor::parse_context(std::list<const_buffer> buffer_list,
                                        unsigned concurrency) {
        if (!impl_) {
            impl_ = { new impl{}, impl_deleter{} };
        }
        impl_->codec_context.emplace(
            impl_->format_context.emplace(
                impl_->io_context.emplace(buffer_list_cursor::create(std::move(buffer_list))),
                source::format{}),
            media::type::video,
            concurrency);
    }

    bool frame_segmentor::codec_valid() const noexcept {
        return impl_->codec_context->valid();
    }

    bool frame_segmentor::context_valid() const noexcept {
        return impl_ != nullptr
            && impl_->io_context.has_value()
            && impl_->format_context.has_value()
            && impl_->codec_context.has_value();
    }

    bool frame_segmentor::buffer_available() const {
        return impl_->io_context->available();
    }

    bool frame_segmentor::try_read() const {
        auto packet = impl_->format_context->read(media::type::video);
        return !packet.empty();
    }

    detail::vector<media::frame> frame_segmentor::try_consume() const {
        if (impl_->codec_context->valid()) {
            return impl_->codec_context
                        ->decode(impl_->format_context
                                      ->read(media::type::video));
        }
        throw core::stream_drained_error{ __FUNCTION__ };
    }

    auto frame_consume(const pixel_consume& consume) {
        return [&consume](frame frame) {
            pixel_array pixel_array;
            pixel_array.fill(nullptr);
            return [&consume, frame = std::move(frame), pixel_array]() mutable {
                if (!frame.empty()) {
                    pixel_array[0] = frame->data[0];
                    pixel_array[1] = frame->data[1];
                    pixel_array[2] = frame->data[2];
                    const_cast<pixel_consume&>(consume)(pixel_array);
                } else {
                    assert(false);
                }
            };
        };
    }

    bool frame_segmentor::try_consume_once(const pixel_consume& pixel_consume) const {
        auto frame = impl_->try_consume_once();
        if (frame.empty()) {
            return false;
        }
        frame_consume(pixel_consume)(std::move(frame))();
        return true;
    }

    media::frame frame_segmentor::try_consume_once() const {
        return impl_->try_consume_once();
    }

    folly::Future<folly::Function<void()>>
    frame_segmentor::defer_consume_once(const pixel_consume& pixel_consume) const {
        auto* impl = impl_.get();
        return folly::async([impl] {
                   return impl->try_consume_once();
               })
               .filter([](const frame& frame) {
                   return !frame.empty();
               })
               .thenValue(frame_consume(pixel_consume));
    }

    folly::Future<media::frame>
    frame_segmentor::defer_consume_once() const {
        auto* impl = impl_.get();
        return folly::async([impl] {
                return impl->try_consume_once();
            })
            .filter([](const frame& frame) {
                return !frame.empty();
            });
    }
}
