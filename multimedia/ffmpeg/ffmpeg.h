#pragma once

namespace av
{
    namespace pixel
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
    }

    struct media
    {
        using type = AVMediaType;
        struct audio : std::integral_constant<type, AVMEDIA_TYPE_AUDIO> {};
        struct video : std::integral_constant<type, AVMEDIA_TYPE_VIDEO> {};
        struct subtitle : std::integral_constant<type, AVMEDIA_TYPE_SUBTITLE> {};
        struct unknown : std::integral_constant<type, AVMEDIA_TYPE_UNKNOWN> {};
    };

    inline void register_all()
    {
        static std::once_flag once;
        std::call_once(once, [] { av_register_all(); });
    }

    class frame
    {
    public:
        using pointer = AVFrame * ;
        using reference = AVFrame & ;
        bool empty() const;
        pointer operator->() const;
        void unref() const;
        frame();
        explicit frame(std::nullptr_t);
    private:
        std::shared_ptr<AVFrame> handle_;
    };

    class packet
    {
    public:
        using pointer = AVPacket * ;
        using reference = AVPacket & ;
        bool empty() const;
        std::basic_string_view<uint8_t> buffer_view() const;
        std::string_view cbuffer_view() const;
        pointer operator->() const;
        void unref() const;
        packet();
        explicit packet(std::nullptr_t);
        explicit packet(std::basic_string_view<uint8_t> sv); // copy by av_malloc from buffer view
    private:
        std::shared_ptr<AVPacket> handle_;
    };

    struct codec : std::reference_wrapper<AVCodec>
    {
        using pointer = type * ;
        using reference = type & ;
        using parameter = std::reference_wrapper<const AVCodecParameters>;
        pointer operator->() const;
        codec();

        explicit codec(reference ref);
        explicit codec(pointer ptr);
    };

    struct stream : std::reference_wrapper<AVStream>
    {
        using pointer = type * ;
        using reference = type & ;
        int index() const;
        codec::parameter params() const;
        media::type media() const;
        std::pair<int, int> scale() const;
        pointer operator->() const;
        stream();
        explicit stream(reference ref);
        explicit stream(pointer ptr);
    };

    template<typename T>
    decltype(auto) ptr(T&& handle)
    {
        //return static_cast<typename std::decay_t<T>::pointer>(handle);
        return std::forward<T>(handle).operator->();
    }

    struct source
    {
        std::string url;
    };

    struct sink
    {
        std::string url;
    };
}
