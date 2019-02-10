#pragma once
#include <folly/Function.h>
#include <folly/futures/Future.h>
#include <boost/asio/buffer.hpp>
#include <boost/container/small_vector.hpp>

namespace media
{
    class frame;
}

namespace media
{
    namespace detail
    {
        using boost::asio::const_buffer;
        template <typename T>
        using vector = boost::container::small_vector<T, 1>;
    }

    using pixel_array = std::array<uint8_t*, 3>;
    using pixel_consume = folly::Function<void(pixel_array&)>;

    class frame_segmentor final
    {
        struct impl;
        struct impl_deleter final : private std::default_delete<impl>
        {
            void operator()(impl* impl);
        };

        std::unique_ptr<impl, impl_deleter> impl_;

    public:
        frame_segmentor() = default;
        frame_segmentor(const frame_segmentor&) = default;
        frame_segmentor(frame_segmentor&&) noexcept = default;
        frame_segmentor& operator=(const frame_segmentor&) = default;
        frame_segmentor& operator=(frame_segmentor&&) noexcept = default;
        ~frame_segmentor() = default;

        explicit frame_segmentor(std::list<detail::const_buffer> buffer_list, unsigned concurrency);

        explicit operator bool() const;

        void parse_context(std::list<detail::const_buffer> buffer_list, unsigned concurrency);
        bool codec_available() const noexcept;
        bool context_valid() const noexcept;
        bool buffer_available() const;
        bool try_read() const;
        detail::vector<media::frame> try_consume(bool drop_packet = false) const;
        bool try_consume_once(const pixel_consume& pixel_consume) const;
        media::frame try_consume_once() const;
        folly::Future<folly::Function<void()>> defer_consume_once(const pixel_consume& pixel_consume) const;
        folly::Future<media::frame> defer_consume_once() const;
    };
}
