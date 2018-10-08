#include "stdafx.h"
#include "component.h"

namespace media::component
{
    namespace detail
    {
        using boost::beast::multi_buffer;
        template<typename T>
        using vector = boost::container::small_vector<T, 2>;
    }

    using namespace detail;

    struct frame_segmentor::impl
    {
        int64_t consume_count = 0;
        multi_buffer init_buffer;
        multi_buffer current_buffer;
        std::shared_ptr<io_base> cursor;
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

    frame_segmentor::frame_segmentor(std::list<const_buffer> buffer_list, unsigned concurrency)
        : impl_(std::make_shared<impl>()) {
        parse_context(std::move(buffer_list), concurrency);
    }

    void frame_segmentor::parse_context(std::list<const_buffer> buffer_list, unsigned concurrency) {
        if (!impl_) {
            impl_ = std::make_shared<impl>();
        }
        impl_->cursor = buffer_list_cursor::create(std::move(buffer_list));
        impl_->codec_context.emplace(
            impl_->format_context.emplace(
                impl_->io_context.emplace(impl_->cursor)),
            media::type::video,
            concurrency);
    }

    bool frame_segmentor::context_valid() const noexcept {
        return impl_ != nullptr
            && impl_->io_context.has_value()
            && impl_->format_context.has_value()
            && impl_->codec_context.has_value();
    }

    bool frame_segmentor::buffer_available() const {
        return impl_->cursor->available();
    }

    void frame_segmentor::reset_buffer_list(std::list<const_buffer> buffer_list) {
        [[maybe_unused]] auto old_cursor = std::exchange(impl_->cursor, buffer_list_cursor::create(std::move(buffer_list)));
        [[maybe_unused]] auto old_cursor2 = impl_->io_context->exchange_cursor(impl_->cursor);
        assert(old_cursor == old_cursor2);
    }

    bool frame_segmentor::try_read() {
        auto packet = impl_->format_context->read(media::type::video);
        return !packet.empty();
    }

    int frame_segmentor::try_consume() {
        if (impl_->codec_context->valid()) {
            auto packet = impl_->format_context->read(media::type::video);
            auto frames = impl_->codec_context->decode(packet);
            return boost::numeric_cast<int>(frames.size());
        }
        return -1;
    }

    auto frame_consume(const pixel_consume& consume) {
        return [&consume](frame&& frame) {
            if (frame.empty()) {
                return false;
            }
            if (consume) {
                const_cast<pixel_consume&>(consume)(pixel_array{ frame->data[0],frame->data[1],frame->data[2] });
            }
            return true;
        };
    }

    bool frame_segmentor::try_consume_once(const pixel_consume& pixel_consume) {
        auto frame = impl_->try_consume_once();
        return frame_consume(pixel_consume)(std::move(frame));
    }

    folly::SemiFuture<bool> frame_segmentor::defer_consume_once(const pixel_consume& pixel_consume) {
        auto impl = impl_.get();
        return folly::async([impl] { return impl->try_consume_once(); })
            .semi()
            .deferValue(frame_consume(pixel_consume));
    }
}
