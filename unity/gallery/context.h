#pragma once
#include "graphic.h"

template<typename T>
class alternate final
{
    T value_ = 0;
    mutable std::optional<T> alternate_;

public:
    constexpr explicit alternate(T&& value)
        : value_(std::forward<T>(value)) {}

    constexpr T value() const {
        return alternate_.value_or(value_);
    }

    std::optional<T> alter(const T& alter) const {
        return std::exchange(alternate_,
                             std::make_optional(alter));
    }
};

struct stream_context final
{
    graphic::texture_array texture_array{};
    folly::MPMCQueue<media::frame> decode_queue{ 180 };
    folly::ProducerConsumerQueue<media::frame> render_queue{ 180 };
    media::frame* avail_frame = nullptr;

    struct update final
    {
        int64_t decode_try = 0;
        int64_t decode_success = 0;
        int64_t render_finish = 0;
        std::bitset<3> texture_state{ 0 };

        struct event final
        {
            int64_t begin = 0;
            int64_t end = 0;
        } event;
    } update;

    int col = 0;
    int row = 0;
    bool stop = false;
    int64_t decode_count = 0;
    int64_t update_pending_count = 0;
    int width_offset = 0;
    int height_offset = 0;
    int index = 0;

    stream_context() = default;
    stream_context(const stream_context&) = delete;
    stream_context(stream_context&&) = delete;
    stream_context& operator=(const stream_context&) = delete;
    stream_context& operator=(stream_context&&) = delete;
    ~stream_context() = default;
};

struct update_batch final
{
    struct tile_offset
    {
        int width_offset = 0;
        int height_offset = 0;
    };

    struct tile_render_context final : tile_offset
    {
        media::frame frame{ nullptr };
        graphic::texture_array* texture_array = nullptr;
        int64_t frame_index = 0;
        int64_t batch_index = 0;
    };
};
