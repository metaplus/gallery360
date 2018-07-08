#pragma once

namespace av
{
    struct pixel
    {
        using type = AVPixelFormat;
        struct nv12 : std::integral_constant<type, AV_PIX_FMT_NV12> {};
        struct nv21 : std::integral_constant<type, AV_PIX_FMT_NV21> {};
        struct rgb24 : std::integral_constant<type, AV_PIX_FMT_RGB24> {};
        struct rgba : std::integral_constant<type, AV_PIX_FMT_RGBA> {};
        struct yuv420 : std::integral_constant<type, AV_PIX_FMT_YUV420P> {};
        struct yuv422 : std::integral_constant<type, AV_PIX_FMT_YUV422P> {};
        struct uyvy : std::integral_constant<type, AV_PIX_FMT_UYVY422> {};
        struct yuyv : std::integral_constant<type, AV_PIX_FMT_YUYV422> {};
        struct yvyu : std::integral_constant<type, AV_PIX_FMT_YVYU422> {};
    };

    struct media
    {
        using type = AVMediaType;
        struct audio : std::integral_constant<type, AVMEDIA_TYPE_AUDIO> {};
        struct video : std::integral_constant<type, AVMEDIA_TYPE_VIDEO> {};
        struct subtitle : std::integral_constant<type, AVMEDIA_TYPE_SUBTITLE> {};
        struct unknown : std::integral_constant<type, AVMEDIA_TYPE_UNKNOWN> {};
    };

    void register_all();

    class frame
    {
    public:
        using pointer = AVFrame * ;
        using reference = AVFrame & ;
        frame();
        explicit frame(std::nullptr_t);
        pointer operator->() const;
        bool empty() const;
        void unref() const;
    private:
        std::shared_ptr<AVFrame> handle_;
    };

    class packet
    {
    public:
        using pointer = AVPacket * ;
        using reference = AVPacket & ;
        packet();
        explicit packet(std::nullptr_t);
        explicit packet(std::basic_string_view<uint8_t> sv); // copy by av_malloc from buffer view
        explicit packet(std::string_view csv);
        pointer operator->() const;
        explicit operator bool() const;
        bool empty() const;
        size_t size() const;
        std::basic_string_view<uint8_t> bufview() const;
        void unref() const;
    private:
        std::shared_ptr<AVPacket> handle_;
        struct chunk;
    };

    struct codec : std::reference_wrapper<AVCodec>
    {
        using pointer = type * ;
        using reference = type & ;
        using parameter = std::reference_wrapper<const AVCodecParameters>;
        codec();
        explicit codec(reference ref);
        explicit codec(pointer ptr);
        pointer operator->() const;
    };

    struct stream : std::reference_wrapper<AVStream>
    {
        using pointer = type * ;
        using reference = type & ;
        stream();
        explicit stream(reference ref);
        explicit stream(pointer ptr);
        pointer operator->() const;
        int index() const;
        codec::parameter params() const;
        media::type media() const;
        std::pair<int, int> scale() const;
    };

    struct source
    {
        struct format : std::string_view
        {
            using std::string_view::string_view;
            using std::string_view::operator=;
        };
        struct path : std::string_view
        {
            using std::string_view::string_view;
            using std::string_view::operator=;
        };
    };

    struct sink
    {
        struct format : std::string_view
        {
            using std::string_view::string_view;
            using std::string_view::operator=;
        };
        struct path : std::string_view
        {
            using std::string_view::string_view;
            using std::string_view::operator=;
        };
    };
}