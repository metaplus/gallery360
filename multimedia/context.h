#pragma once
#include "media.h"
#include "io.cursor.h"
#include <boost/container/small_vector.hpp>
#include <memory>

namespace media
{
    class io_context final : io_context_base
    {
        using value_type = AVIOContext;
        using pointer = AVIOContext *;

        struct deleter final
        {
            void operator()(pointer context) const;
        };

        std::unique_ptr<io_base> io_base_;
        std::unique_ptr<AVIOContext, deleter> io_handle_;

    public:
        static constexpr inline size_t default_cache_page_size = 4096;
        static constexpr inline bool default_buffer_writable = false;

        io_context() = default;
        io_context(io_context const&) = default;
        io_context(io_context&&) noexcept = default;

        io_context& operator=(io_context const&) = default;
        io_context& operator=(io_context&&) noexcept = default;

        explicit io_context(std::unique_ptr<io_base> io_cursor);
        io_context(read_context&& read, write_context&& write, seek_context&& seek);

        pointer operator->() const;
        explicit operator bool() const;

        bool available() const noexcept;

    private:
        static int on_read_buffer(void* opaque, uint8_t* buffer, int size);
        static int on_write_buffer(void* opaque, uint8_t* buffer, int size);
        static int64_t on_seek_stream(void* opaque, int64_t offset, int whence);
    };

    class format_context final
    {
        using value_type = AVFormatContext;
        using pointer = AVFormatContext *;

        struct deleter final
        {
            void operator()(pointer context) const;
        };

        std::unique_ptr<AVFormatContext, deleter> format_handle_;
        std::reference_wrapper<io_context> io_handle_;

    public:
        format_context(io_context& io, source::format iformat);
        format_context(io_context& io, sink::format oformat);
        explicit format_context(source::path ipath);
        explicit format_context(sink::path opath);

        format_context() = default;
        format_context(format_context const&) = default;
        format_context(format_context&&) noexcept = default;
        format_context& operator=(format_context const&) = default;
        format_context& operator=(format_context&&) noexcept = default;
        pointer operator->() const;
        explicit operator bool() const;

        stream demux(media::type media_type) const;
        std::pair<codec, stream> demux_with_codec(media::type media_type) const;
        packet read(media::type media_type) const;
        std::vector<packet> read(size_t count, media::type media_type) const;
    };

    namespace detail
    {
        template <typename T>
        using vector = boost::container::small_vector<T, 1>;
    }

    class codec_context final
    {
        using value_type = AVCodecContext;
        using pointer = AVCodecContext *;
        using resolution = std::pair<decltype(AVCodecContext::width), decltype(AVCodecContext::height)>;

        struct deleter final
        {
            void operator()(pointer context) const;
        };

        std::unique_ptr<AVCodecContext, deleter> codec_handle_;
        stream format_stream_;
        mutable int64_t dispose_count_ = 0;
        mutable bool flushed_ = false;

    public:
        codec_context(codec codec, stream stream, unsigned threads);
        codec_context(format_context& format, media::type type, unsigned threads);

        codec_context() = default;
        codec_context(codec_context const&) = default;
        codec_context(codec_context&&) noexcept = default;
        codec_context& operator=(codec_context const&) = default;
        codec_context& operator=(codec_context&&) noexcept = default;
        pointer operator->() const;
        explicit operator bool() const;

        bool valid() const;
        int64_t dispose_count() const;
        int64_t frame_count() const;
        detail::vector<frame> decode(const packet& compressed) const;
    };
}
