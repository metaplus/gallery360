#include "stdafx.h"
#include "context.h"

av::format_context::format_context(std::variant<source, sink> io)
    : handle_(nullptr)
{
    std::visit([this](auto&& arg) constexpr
    {
        register_all();
        if constexpr (std::is_same_v<source, std::decay_t<decltype(arg)>>)
        {
            pointer ptr = nullptr;
            core::verify(avformat_open_input(&ptr, arg.url.c_str(), nullptr, nullptr));
            handle_.reset(ptr, [](pointer p) { avformat_close_input(&p); });
            core::verify(avformat_find_stream_info(ptr, nullptr));   // 60ms+
#ifdef _DEBUG
            av_dump_format(ptr, 0, ptr->filename, 0);
#endif
        }
        else
        {
            // TODO: SINK BRANCH
            throw core::not_implemented_error{};
        }
    }, io);
}

av::format_context::pointer av::format_context::operator->() const
{
    return handle_.get();
}

av::stream av::format_context::demux(const media::type media_type) const
{
    return stream{ handle_->streams[av_find_best_stream(handle_.get(), media_type, -1, -1, nullptr, 0)] };
}

std::pair<av::codec, av::stream> av::format_context::demux_with_codec(const media::type media_type) const
{
    codec::pointer cdc = nullptr;
    const auto ptr = handle_.get();
    const auto index = av_find_best_stream(ptr, media_type, -1, -1, &cdc, 0);
    return std::make_pair(codec{ cdc }, stream{ ptr->streams[index] });
}

av::packet av::format_context::read(const std::optional<media::type> media_type) const
{
    packet pkt;
    while (av_read_frame(handle_.get(), get_pointer(pkt)) == 0
        && media_type.has_value()
        && handle_->streams[pkt->stream_index]->codecpar->codec_type != media_type)
    {
        pkt.unref();
    }
    return pkt;
}

std::vector<av::packet> av::format_context::read(const size_t count, std::optional<media::type> media_type) const
{
    std::vector<packet> packets; packets.reserve(count);
    //std::generate_n(std::back_inserter(packets), count, std::bind(&format_context::read, this, media_type));
    std::generate_n(std::back_inserter(packets), count, [this, media_type] { return read(media_type); });
    return packets;
}

av::codec_context::codec_context(codec codec, const stream stream, const unsigned threads)
    : handle_(avcodec_alloc_context3(get_pointer(codec)), [](pointer p) { avcodec_free_context(&p); })
    , stream_(stream)
    , state_()
{
    core::verify(avcodec_parameters_to_context(handle_.get(), stream_->codecpar));
    core::verify(av_opt_set_int(handle_.get(), "refcounted_frames", 1, 0));
    core::verify(av_opt_set_int(handle_.get(), "threads", threads, 0));
    core::verify(avcodec_open2(handle_.get(), get_pointer(codec), nullptr));
}

av::codec_context::pointer av::codec_context::operator->() const
{
    return handle_.get();
}

bool av::codec_context::valid() const
{
    return !state_.flushed;
}

int64_t av::codec_context::decoded_count() const
{
    return state_.count;
}

int64_t av::codec_context::frame_count() const
{
    return stream_->nb_frames;
}

std::vector<av::frame> av::codec_context::decode(const packet& packets) const
{
    if (std::exchange(state_.flushed, packets.empty()))
        throw std::logic_error{ "prohibit multiple codec context flush" };
    if (stream_.index() != packets->stream_index)
        throw std::invalid_argument{ "prohibt decode disparate stream" };
    std::vector<frame> decoded_frames;
    decoded_frames.reserve(packets.empty() ? 10 : 1);
    core::verify(avcodec_send_packet(get_pointer(handle_), get_pointer(packets)));
    frame current;
    while (avcodec_receive_frame(get_pointer(handle_), get_pointer(current)) == 0)
        decoded_frames.push_back(std::exchange(current, frame{}));
    state_.count += decoded_frames.size();
    return decoded_frames;
}