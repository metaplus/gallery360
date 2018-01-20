#pragma once
namespace av
{
    namespace pixel
    {
        struct base { using numeric=std::underlying_type_t<AVPixelFormat>; };
        template<typename T> constexpr bool is_valid=std::is_base_of_v<base,T>;
        struct nv12 :base,std::integral_constant<base::numeric,AV_PIX_FMT_NV12>{};
        struct nv21 :base,std::integral_constant<base::numeric,AV_PIX_FMT_NV21>{};
        struct rgb24 :base,std::integral_constant<base::numeric,AV_PIX_FMT_RGB24>{};
        struct rgba :base,std::integral_constant<base::numeric,AV_PIX_FMT_RGBA>{};
        struct yuv420 :base,std::integral_constant<base::numeric,AV_PIX_FMT_YUV420P>{};
        struct yuv422 :base,std::integral_constant<base::numeric,AV_PIX_FMT_YUV422P>{};
        struct uyvy :base,std::integral_constant<base::numeric,AV_PIX_FMT_UYVY422>{};
        struct yuyv :base,std::integral_constant<base::numeric,AV_PIX_FMT_YUYV422>{};
        struct yvyu :base,std::integral_constant<base::numeric,AV_PIX_FMT_YVYU422>{};
    }
    namespace media
    {
        struct base { using numeric=std::underlying_type_t<AVMediaType>; };
        template<typename T> constexpr bool is_valid=std::is_base_of_v<base,T>;
        struct audio :base,std::integral_constant<base::numeric,AVMEDIA_TYPE_AUDIO>{};
        struct video :base,std::integral_constant<base::numeric,AVMEDIA_TYPE_VIDEO>{};
        struct subtitle :base,std::integral_constant<base::numeric,AVMEDIA_TYPE_SUBTITLE>{};
        struct unknown :base,std::integral_constant<base::numeric,AVMEDIA_TYPE_UNKNOWN>{};
        struct all :base{};
    }
    inline void register_all()
    {
        static std::once_flag once;
        std::call_once(once,[]{ av_register_all(); });
    }
    class frame
    {
        std::shared_ptr<AVFrame> handle_;
    public:
        using pointer=AVFrame*;
        frame() :handle_(av_frame_alloc(),[](pointer p){ av_frame_free(&p); }){}
        auto empty() const { return handle_==nullptr||handle_->data==nullptr||handle_->data[0]==nullptr; }
        auto operator->() const { return handle_.get(); }
        void unref() const { av_frame_unref(handle_.get()); }
    };
    class packet
    {
        std::shared_ptr<AVPacket> handle_;
    public:
        using pointer=AVPacket*;
        packet() :handle_(av_packet_alloc(),[](pointer p){ av_packet_free(&p); }){}
        auto empty() const { return handle_==nullptr||handle_->data==nullptr||handle_->size<=0; }
        auto operator->() const { return handle_.get(); }
        void unref() const { av_packet_unref(handle_.get()); }
    };
    struct stream
    {
        using pointer=AVStream*;
        pointer ptr;
        auto index() const { return ptr->index; }
        auto params() const { return ptr->codecpar; }
        auto media() const { return params()->codec_type; }
        auto scale() const { return std::make_pair(params()->width,params()->height); } //use structure binding to get seperated dimension
        auto operator->() const { return ptr; }    //AVStream* itself useless
    };
    struct codec
    {
        using pointer=AVCodec*;
        pointer ptr;
        auto operator->() const { return ptr; }
    };
    //template<typename T>
    inline auto ptr=[](auto&& handle)
    {      
        //return static_cast<typename std::decay_t<T>::pointer>(handle);
        return handle.operator->();
    };
    struct source
    {
        std::string url;
    };
    struct sink
    {
        std::string url;
    };
}
