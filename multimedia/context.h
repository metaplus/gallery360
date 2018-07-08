#pragma once

namespace av
{
    class io_context
    {
    public:
        using value_type = AVIOContext;
        using pointer = AVIOContext * ;
        using read_functor = folly::Function<int(uint8_t*, int)>;
        using write_functor = folly::Function<int(uint8_t*, int)>;
        using seek_functor = folly::Function<int64_t(int64_t, int)>;
        using io_functors = std::tuple<read_functor, write_functor, seek_functor>;

        static constexpr inline size_t default_cache_page_size = 4096;
        static constexpr inline bool default_buffer_writable = false;

        io_context() = default;
        io_context(io_functors&& io_functions, uint32_t buf_size, bool buf_writable);
        io_context(read_functor&& read, write_functor&& write, seek_functor&& seek);

        pointer operator->() const;
        explicit operator bool() const;

    private:
        struct io_interface
        {
            virtual ~io_interface() = default;
            virtual int read(uint8_t* buffer, int size) = 0;
            virtual int write(uint8_t* buffer, int size) = 0;
            virtual int64_t seek(int64_t offset, int whence) = 0;
            virtual bool readable() = 0;
            virtual bool writable() = 0;
            virtual bool seekable() = 0;
        };

        std::shared_ptr<io_interface> io_interface_;
        std::shared_ptr<AVIOContext> io_handle_;

        static std::shared_ptr<io_interface> make_io_interface(io_functors&& io_functions);
        static std::shared_ptr<io_interface> make_io_interface(read_functor&& read, write_functor&& write, seek_functor&& seek);

        static int on_read_buffer(void* opaque, uint8_t* buffer, int size);
        static int on_write_buffer(void* opaque, uint8_t* buffer, int size);
        static int64_t on_seek_stream(void* opaque, int64_t offset, int whence);
    };

    class format_context
    {
    public:
        using value_type = AVFormatContext;
        using pointer = AVFormatContext * ;

        format_context() = default;
        format_context(io_context io, source::format iformat);
        format_context(io_context io, sink::format oformat);
        explicit format_context(source::path ipath);
        explicit format_context(sink::path opath);

        pointer operator->() const;
        explicit operator bool() const;

        stream demux(media::type media_type) const;
        std::pair<codec, stream> demux_with_codec(media::type media_type) const;
        packet read(media::type media_type) const;
        std::vector<packet> read(size_t count, media::type media_type) const;

    private:
        //void prepare() const;
        std::shared_ptr<AVFormatContext> format_handle_;
        io_context io_handle_;
    };

    using io_functors = io_context::io_functors;

    class codec_context
    {
    public:
        using value_type = AVCodecContext;
        using pointer = AVCodecContext * ;
        using resolution = std::pair<decltype(AVCodecContext::width), decltype(AVCodecContext::height)>;

        codec_context() = default;
        codec_context(codec codec, stream stream, unsigned threads = boost::thread::hardware_concurrency());
        codec_context(format_context& format, media::type media_type, unsigned threads = boost::thread::hardware_concurrency());

        pointer operator->() const;
        explicit operator bool() const;

        bool valid() const;
        int64_t decoded_count() const;
        int64_t frame_count() const;
        std::vector<frame> decode(const packet& compressed) const;

    private:
        std::shared_ptr<AVCodecContext> codec_handle_;
        stream stream_;

        struct status
        {
            int64_t count;
            bool flushed;
        } mutable status_;
    };
}
