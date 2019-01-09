#pragma once
#include "graphic.h"

struct stream_context final
{
    graphic::texture_array texture_array = {};
    core::dimension offset;
    int index = 0;
    core::coordinate coordinate;

    struct decode_event final
    {
        std::shared_ptr<folly::MPMCQueue<media::frame>> queue;
        int64_t count = 0;
    } decode;

    struct update_event final
    {
        int64_t decode_try = 0;
        int64_t decode_success = 0;
        int64_t render_finish = 0;
        std::bitset<3> texture_state{ 0 };
    } update;

    struct render_event final
    {
        media::frame* frame = nullptr;
        std::shared_ptr<folly::ProducerConsumerQueue<media::frame>> queue;
        int64_t begin = 0;
        int64_t end = 0;
    } render;

    explicit stream_context(int decode_capacity,
                            int render_capacity = 5) {
        decode.queue = std::make_shared<folly::MPMCQueue<media::frame>>(decode_capacity);
        render.queue = std::make_shared<folly::ProducerConsumerQueue<media::frame>>(render_capacity);
    }

    stream_context() = delete;
    stream_context(const stream_context&) = delete;
    stream_context(stream_context&&) = default;
    stream_context& operator=(const stream_context&) = delete;
    stream_context& operator=(stream_context&&) = default;
    ~stream_context() = default;
};

static_assert(!std::is_copy_constructible<stream_context>::value);
static_assert(std::is_move_constructible<stream_context>::value);

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

struct frame_batch;

template <typename T>
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
