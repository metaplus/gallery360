#pragma once
#include "core/spatial.hpp"
#include "multimedia/media.h"
#include <folly/MPMCQueue.h>
#include <readerwriterqueue/readerwriterqueue.h>
#include <bitset>

namespace plugin
{
    struct stream_base
    {
        int index = 0;
        core::coordinate coordinate;
        core::dimension offset;
    };

    struct stream_options final : stream_base
    {
        size_t decode_capacity = 30;
        size_t render_capacity = 1;

        stream_options& with_index(int index) {
            this->index = index;
            return *this;
        }

        stream_options& with_coordinate(int col, int row) {
            this->coordinate = { col, row };
            return *this;
        }

        stream_options& with_offset(int width, int height) {
            this->offset = { width, height };
            return *this;
        }

        stream_options& with_capacity(size_t decode, size_t render) {
            this->decode_capacity = decode;
            this->render_capacity = render;
            return *this;
        }
    };

    struct stream_context final : stream_base
    {
        using decode_frame = std::variant<media::frame, std::exception_ptr>;
        struct decode_event final
        {
            folly::MPMCQueue<decode_frame> queue;
            int64_t enqueue = 0;

            explicit decode_event(const size_t capacity)
                : queue{ capacity } {}
        } decode;

        using update_frame = media::frame;
        struct update_event final
        {
            int64_t dequeue_try = 0;
            int64_t dequeue_success = 0;
            int64_t render_finish = 0;
            std::bitset<3> texture_state{ 0 };
        } update;

        struct render_event final
        {
            media::frame* frame = nullptr;
            moodycamel::ReaderWriterQueue<media::frame> queue;
            int64_t begin = 0;
            int64_t end = 0;

            explicit render_event(const size_t capacity)
                : queue{ capacity } {}
        } render;

        explicit stream_context(stream_options options)
            : stream_base{ options.index, options.coordinate, options.offset }
            , decode{ { options.decode_capacity } }
            , render{ options.render_capacity } { }

        stream_context() = delete;
        stream_context(const stream_context&) = delete;
        stream_context(stream_context&&) = delete;
        stream_context& operator=(const stream_context&) = delete;
        stream_context& operator=(stream_context&&) = delete;
        ~stream_context() = default;
    };

    static_assert(!std::is_copy_constructible<stream_context>::value);
    static_assert(!std::is_move_constructible<stream_context>::value);
}
