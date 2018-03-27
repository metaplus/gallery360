#pragma once

namespace av
{
    class format_context
    {
    public:
        using value_type = AVFormatContext;
        using pointer = AVFormatContext * ;
        pointer operator->() const;
        stream demux(media::type media_type) const;
        std::pair<codec, stream> demux_with_codec(media::type media_type) const;
        packet read(std::optional<media::type> media_type = std::nullopt) const;
        format_context() = default;
        explicit format_context(std::variant<source, sink> io);
    private:
        //void prepare() const;
    private:
        std::shared_ptr<AVFormatContext> handle_;
    };

    class codec_context
    {
    public:
        using value_type = AVCodecContext;
        using pointer = AVCodecContext * ;
        using resolution = std::pair<decltype(AVCodecContext::width), decltype(AVCodecContext::height)>;
        pointer operator->() const;
        bool valid() const;
        int64_t decoded_count() const;
        int64_t frame_count() const;
        std::vector<frame> decode(const packet& compressed) const;
        codec_context() = default;
        codec_context(codec codec, stream stream, unsigned threads = std::thread::hardware_concurrency());
    private:
        std::shared_ptr<AVCodecContext> handle_;
        stream stream_;
        struct state
        {
            int64_t count;
            bool flushed;
        };
        mutable state state_{};
    };
}
