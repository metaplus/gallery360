#include "stdafx.h"
#include "context.h"

av::format_context::format_context(std::variant<source, sink> io)
    :handle_(nullptr)
{
    std::visit([this](auto&& arg)
    {
        register_all();
        if constexpr (std::is_same_v<av::source, std::decay_t<decltype(arg)>>)
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
            throw std::bad_variant_access{};
        }
    }, io);
}

av::format_context::pointer av::format_context::operator->() const
{
    return handle_.get();
}

av::codec_context::codec_context(codec cdc, stream srm, unsigned threads)
    :handle_(avcodec_alloc_context3(ptr(cdc)), [](pointer p) { avcodec_free_context(&p); })
    , stream_(srm)
    , state_()
{
    core::verify(avcodec_parameters_to_context(handle_.get(), stream_.params()));
    core::verify(av_opt_set_int(handle_.get(), "refcounted_frames", 1, 0));
    core::verify(av_opt_set_int(handle_.get(), "threads", threads, 0));
    core::verify(avcodec_open2(handle_.get(), ptr(cdc), nullptr));
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

std::vector<av::frame> av::codec_context::decode(const packet& compressed)
{
    if (std::exchange(state_.flushed, compressed.empty()))
        throw std::runtime_error{ "prohibit multiple codec context flush" };
    if (stream_.index() != compressed->stream_index)
        throw std::invalid_argument{ "prohibt decode disparate stream" };
    std::vector<frame> decodeds;
    if (compressed.empty())
        decodeds.reserve(10);
    core::verify(avcodec_send_packet(ptr(handle_), ptr(compressed)));
    frame current;
    while (avcodec_receive_frame(ptr(handle_), ptr(current)) == 0)
        decodeds.push_back(std::exchange(current, frame{}));
    state_.count += decodeds.size();
    return decodeds;
}