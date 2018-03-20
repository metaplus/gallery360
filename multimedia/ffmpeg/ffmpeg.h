#pragma once

namespace av
{
    namespace pixel
    {
        struct base { using type = std::underlying_type_t<AVPixelFormat>; };
        template<typename T> constexpr bool is_valid = std::is_base_of_v<base, T>;
        struct nv12 : base, std::integral_constant<base::type, AV_PIX_FMT_NV12> {};
        struct nv21 : base, std::integral_constant<base::type, AV_PIX_FMT_NV21> {};
        struct rgb24 : base, std::integral_constant<base::type, AV_PIX_FMT_RGB24> {};
        struct rgba : base, std::integral_constant<base::type, AV_PIX_FMT_RGBA> {};
        struct yuv420 : base, std::integral_constant<base::type, AV_PIX_FMT_YUV420P> {};
        struct yuv422 : base, std::integral_constant<base::type, AV_PIX_FMT_YUV422P> {};
        struct uyvy : base, std::integral_constant<base::type, AV_PIX_FMT_UYVY422> {};
        struct yuyv : base, std::integral_constant<base::type, AV_PIX_FMT_YUYV422> {};
        struct yvyu : base, std::integral_constant<base::type, AV_PIX_FMT_YVYU422> {};
    }

    namespace media
    {
        struct base { using type = std::underlying_type_t<AVMediaType>; };
        template<typename T> constexpr bool is_valid = std::is_base_of_v<base, T>;
        struct audio : base, std::integral_constant<base::type, AVMEDIA_TYPE_AUDIO> {};
        struct video : base, std::integral_constant<base::type, AVMEDIA_TYPE_VIDEO> {};
        struct subtitle : base, std::integral_constant<base::type, AVMEDIA_TYPE_SUBTITLE> {};
        struct unknown : base, std::integral_constant<base::type, AVMEDIA_TYPE_UNKNOWN> {};
        struct all : base {};
    }

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
        explicit frame(std::nullptr_t) : handle_() {}
        frame() :handle_(av_frame_alloc(), [](pointer p) { av_frame_free(&p); }) {}
        bool empty() const { return handle_ == nullptr || handle_->data == nullptr || handle_->data[0] == nullptr; }
        pointer operator->() const { return handle_.get(); }
        void unref() const { av_frame_unref(handle_.get()); }
    private:
        std::shared_ptr<AVFrame> handle_;
    };

    class packet
    {
    public:
        using pointer = AVPacket * ;
        using reference = AVPacket & ;
        explicit packet(std::nullptr_t) : handle_() {}
        packet() :handle_(av_packet_alloc(), [](pointer p) { av_packet_free(&p); }) {}
        bool empty() const { return handle_ == nullptr || handle_->data == nullptr || handle_->size <= 0; }
        pointer operator->() const { return handle_.get(); }
        void unref() const { av_packet_unref(handle_.get()); }
    private:
        std::shared_ptr<AVPacket> handle_;
    };

    struct stream : std::reference_wrapper<AVStream>
    {
        using pointer = type * ;
        using reference = type & ;
        explicit stream(reference ref) : reference_wrapper(ref) {}
        explicit stream(const pointer ptr) : reference_wrapper(*ptr) {}
        auto index() const { return get().index; }
        auto params() const { return get().codecpar; }
        auto media() const { return params()->codec_type; }
        auto scale() const { return std::make_pair(params()->width, params()->height); } //use structure binding to get seperated dimension
        pointer operator->() const { return std::addressof(get()); }
        stream() : reference_wrapper(core::make_null_reference_wrapper<type>()) {};
    };

    struct codec : std::reference_wrapper<AVCodec>
    {
        using pointer = type * ;
        using reference = type & ;
        explicit codec(reference ref) : reference_wrapper(ref) {}
        explicit codec(const pointer ptr) : reference_wrapper(*ptr) {}
        pointer operator->() const { return std::addressof(get()); }
        codec() : reference_wrapper(core::make_null_reference_wrapper<type>()) {};
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
