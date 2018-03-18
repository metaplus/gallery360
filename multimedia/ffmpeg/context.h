#pragma once

namespace av
{
    class format_context
    {
    public:
        using value_type = AVFormatContext;
        using pointer = AVFormatContext * ;
        format_context() = default;
        explicit format_context(std::variant<source, sink> io);
        pointer operator->() const;
        template<typename Media>
        std::pair<stream, codec> demux();
        template<typename Media = media::all>
        packet read();  //default template parameter entails unbiased reading
    private:
        //void prepare() const;
    private:
        std::shared_ptr<AVFormatContext> handle_;
    };

    template <typename Media>
    std::pair<stream, codec> format_context::demux()
    {
        static_assert(media::is_valid<Media>);
        //prepare();
        codec::pointer cdc = nullptr;
        const auto ptr = handle_.get();
        const auto index = av_find_best_stream(ptr,
            static_cast<AVMediaType>(Media::value), -1, -1, &cdc, 0);
        return std::make_pair(stream{ ptr->streams[index] }, codec{ cdc });
    }

    template <typename Media>
    packet format_context::read()
    {
        packet pkt;
        //prepare();
        static_assert(media::is_valid<Media>);
        while (av_read_frame(handle_.get(), ptr(pkt)) == 0)
        {
            if constexpr (std::is_same_v<Media, media::all>)
                break;
            else
            {   
//                static thread_local std::map<decltype(pkt->stream_index), AVMediaType> cache;
//                AVMediaType pkt_type;
//                if (const auto iter = cache.find(pkt->stream_index); iter != cache.end())
//                    pkt_type = iter->second;
//                else
//                {
//                    pkt_type = handle_->streams[pkt->stream_index]->codecpar->codec_type;
//                    cache.emplace(pkt->stream_index, pkt_type);
//                }
                if (handle_->streams[pkt->stream_index]->codecpar->codec_type == static_cast<AVMediaType>(Media::value))
                    break;
                pkt.unref();
            }
        }
        return pkt;
    }

    class codec_context
    {
    public:
        using value_type = AVCodecContext;
        using pointer = AVCodecContext * ;
        using resolution = std::pair<decltype(AVCodecContext::width), decltype(AVCodecContext::height)>;
        codec_context() = default;
        codec_context(codec cdc, stream srm, unsigned threads = std::thread::hardware_concurrency());
        pointer operator->() const;
        bool valid() const;
        int64_t decoded_count() const;      
        int64_t frame_count() const;
        std::vector<frame> decode(const packet& compressed);
    private:
        std::shared_ptr<AVCodecContext> handle_;    
        stream stream_;
        struct state
        {
            int64_t count;
            bool flushed;
        }state_{};
    };
}