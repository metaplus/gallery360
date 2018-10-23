#pragma once                    

namespace media
{
    class frame;
}

namespace media::component
{
    namespace detail
    {
        using boost::asio::const_buffer;
        template<typename T>
        using vector = boost::container::small_vector<T, 1>;
    }

    using pixel_array = std::array<uint8_t*, 3>;
    using pixel_consume = folly::Function<void(pixel_array&)>;

    class frame_segmentor
    {
        struct impl;
        std::shared_ptr<impl> impl_;

    public:
        frame_segmentor() = default;
        frame_segmentor(const frame_segmentor&) = default;
        frame_segmentor(frame_segmentor&&) noexcept = default;
        frame_segmentor& operator=(const frame_segmentor&) = default;
        frame_segmentor& operator=(frame_segmentor&&) noexcept = default;
        ~frame_segmentor() = default;

        explicit frame_segmentor(std::list<detail::const_buffer> buffer_list,
                                 unsigned concurrency = std::thread::hardware_concurrency());
        template<typename ...BufferSequence>
        explicit frame_segmentor(unsigned concurrency, BufferSequence&& ...sequence)
            : frame_segmentor(core::split_buffer_sequence(std::forward<BufferSequence>(sequence)...),
                              concurrency) {}
        explicit operator bool() const;

        void parse_context(std::list<detail::const_buffer> buffer_list, unsigned concurrency);
        bool codec_valid() const noexcept;
        bool context_valid() const noexcept;
        bool buffer_available() const;
        void reset_buffer_list(std::list<detail::const_buffer> buffer_list);
        bool try_read();
        detail::vector<media::frame> try_consume();
        bool try_consume_once(const pixel_consume& pixel_consume) const;
        media::frame try_consume_once() const;
        folly::Future<folly::Function<void()>> defer_consume_once(const pixel_consume& pixel_consume) const;
        folly::Future<media::frame> defer_consume_once() const;
    };
}