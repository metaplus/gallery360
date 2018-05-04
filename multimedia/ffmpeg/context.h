#pragma once

namespace av
{
    class io_context
    {
    public:
        using value_type = AVIOContext;
        using pointer = AVIOContext * ;
        using read_func = std::function<int(uint8_t*, int)>;
        using write_func = std::function<int(uint8_t*, int)>;
        using seek_func = std::function<int64_t(int64_t, int)>;
        using func_tuple = std::tuple<read_func, write_func, seek_func>;
        io_context() = default;
        explicit io_context(func_tuple&& io_functions, uint32_t buf_size = 4096, bool buf_writable = false);
        pointer operator->() const;
        explicit operator bool() const;
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
        static std::shared_ptr<io_interface> make_io_interface(func_tuple&& io_functions = { nullptr,nullptr,nullptr });
    private:
        std::shared_ptr<io_interface> io_interface_;
        std::shared_ptr<AVIOContext> io_handle_;
        static int read_func_delegate(void* opaque, uint8_t* buffer, int size);
        static int write_func_delegate(void* opaque, uint8_t* buffer, int size);
        static int64_t seek_func_delegate(void* opaque, int64_t offset, int whence);
    };

    class format_context
    {
    public:
        using value_type = AVFormatContext;
        using pointer = AVFormatContext * ;
        format_context() = default;
        format_context(io_context io, source::format iformat);
        format_context(io_context io, sink::format iformat);
        explicit format_context(source::path ipath);
        explicit format_context(sink::path opath);
        pointer operator->() const;
        explicit operator bool() const;
        stream demux(media::type media_type) const;
        std::pair<codec, stream> demux_with_codec(media::type media_type) const;
        packet read(std::optional<media::type> media_type) const;
        std::vector<packet> read(size_t count, std::optional<media::type> media_type) const;
    private:
        //void prepare() const;
        std::shared_ptr<AVFormatContext> format_handle_;
        io_context io_handle_;
    };

    class codec_context
    {
    public:
        using value_type = AVCodecContext;
        using pointer = AVCodecContext * ;
        using resolution = std::pair<decltype(AVCodecContext::width), decltype(AVCodecContext::height)>;
        codec_context() = default;
        codec_context(codec codec, stream stream, unsigned threads = std::thread::hardware_concurrency());
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
        };
        mutable status status_{};
    };
}
