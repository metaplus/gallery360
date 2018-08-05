#include "stdafx.h"
#include "context.h"

media::io_context::io_context(std::shared_ptr<io_base> io)
    : io_base_(std::move(io))
    , io_handle_(avio_alloc_context(static_cast<uint8_t*>(av_malloc(default_cache_page_size)),
                                    default_cache_page_size, default_buffer_writable, io_base_.get(),
                                    io_base_->readable() ? on_read_buffer : nullptr,
                                    io_base_->writable() ? on_write_buffer : nullptr,
                                    io_base_->seekable() ? on_seek_stream : nullptr),
                 [](pointer ptr) { av_freep(&ptr->buffer);  av_freep(&ptr); })
{
    assert(io_base_ != nullptr);
    assert(io_handle_ != nullptr);
}

media::io_context::io_context(cursor::buffer_type const& buffer)
    : io_context(std::make_shared<random_access_curser>(buffer))
{}

media::io_context::io_context(read_context&& read, write_context&& write, seek_context&& seek)
    : io_context(std::make_shared<generic_cursor>(std::move(read), std::move(write), std::move(seek)))
{}

media::io_context::pointer media::io_context::operator->() const
{
    return io_handle_.get();
}

media::io_context::operator bool() const
{
    return io_handle_ != nullptr && io_base_ != nullptr;
}

int media::io_context::on_read_buffer(void* opaque, uint8_t* buffer, int size)
{
    return static_cast<io_base*>(opaque)->read(buffer, size);
}

int media::io_context::on_write_buffer(void* opaque, uint8_t* buffer, int size)
{
    return static_cast<io_base*>(opaque)->write(buffer, size);
}

int64_t media::io_context::on_seek_stream(void* opaque, int64_t offset, int whence)
{
    return static_cast<io_base*>(opaque)->seek(offset, whence);
}

#ifdef MULTIMEDIA_USE_LEGACY
media::format_context::format_context(std::variant<source, sink> io)
    : format_handle_(nullptr)
    , io_handle_()
{
    std::visit([this](auto&& arg) constexpr
    {
        register_all();
        if constexpr (std::is_same_v<source, std::decay_t<decltype(arg)>>)
        {
            pointer ptr = nullptr;
            core::verify(avformat_open_input(&ptr, arg.url.c_str(), nullptr, nullptr));
            format_handle_.reset(ptr, [](pointer p) { avformat_close_input(&p); });
            core::verify(avformat_find_stream_info(ptr, nullptr));   // 60ms+
        #ifdef _DEBUG
            av_dump_format(ptr, 0, ptr->filename, 0);
        #endif // define _DEBUG
        } else
        {
            // TODO: SINK BRANCH
            throw core::not_implemented_error{};
        }
    }, io);
}
#endif // define MULTIMEDIA_USE_LEGACY

media::format_context::format_context(io_context io, source::format iformat)
    : format_handle_(nullptr)
    , io_handle_(std::move(io))
{
    register_all();
    auto format_ptr = avformat_alloc_context();
    format_ptr->pb = core::get_pointer(io_handle_);
    core::verify(avformat_open_input(&format_ptr, nullptr,
                                     iformat.empty() ? nullptr : av_find_input_format(iformat.data()), nullptr));
    assert(format_ptr != nullptr);
    format_handle_.reset(format_ptr, [](pointer p) { avformat_close_input(&p); });
    core::verify(avformat_find_stream_info(format_ptr, nullptr));
#ifdef _DEBUG
    av_dump_format(format_ptr, 0, format_ptr->url, 0);
#endif
}

media::format_context::format_context(io_context io, sink::format oformat)
{
    throw core::not_implemented_error{ "format_context::constructor" };
}

media::format_context::format_context(io_context io, bool source)
{
    *this = source ?
        format_context{ std::move(io), source::format{""} } :
        format_context{ std::move(io), sink::format{""} };
}

media::format_context::format_context(source::path ipath)
    : format_handle_(nullptr)
{
    register_all();

    pointer ptr = nullptr;
    core::verify(avformat_open_input(&ptr, ipath.data(), nullptr, nullptr));
    format_handle_.reset(ptr, [](pointer p) { avformat_close_input(&p); });
    core::verify(avformat_find_stream_info(ptr, nullptr));   // 60ms+
#ifdef _DEBUG
    av_dump_format(ptr, 0, ptr->url, 0);
#endif
}

media::format_context::format_context(sink::path opath)
{
    throw core::not_implemented_error{ "not_implemented_error" };
}

media::format_context::pointer media::format_context::operator->() const
{
    return format_handle_.get();
}

media::format_context::operator bool() const
{
    return format_handle_ != nullptr;
}

media::stream media::format_context::demux(media::type media_type) const
{
    return stream{ format_handle_->streams[
        av_find_best_stream(format_handle_.get(), static_cast<AVMediaType>(media_type), -1, -1, nullptr, 0)] };
}

std::pair<media::codec, media::stream> media::format_context::demux_with_codec(media::type media_type) const
{
    codec::pointer cdc = nullptr;
    const auto format_ptr = format_handle_.get();
    const auto index = av_find_best_stream(format_ptr, static_cast<AVMediaType>(media_type), -1, -1, &cdc, 0);
    return std::make_pair(codec{ cdc }, stream{ format_ptr->streams[index] });
}

media::packet media::format_context::read(media::type media_type) const
{
    media::packet packet;
    while (av_read_frame(format_handle_.get(), core::get_pointer(packet)) == 0
           && media_type != media::type::unknown
           && !core::underlying_same(media_type, format_handle_->streams[packet->stream_index]->codecpar->codec_type))
    {
        packet.unref();
    }
    return packet;
}

std::vector<media::packet> media::format_context::read(const size_t count, media::type media_type) const
{
    std::vector<packet> packets; packets.reserve(count);
    std::generate_n(std::back_inserter(packets), count,
                    [this, media_type] { return read(media_type); });
    return packets;
}

media::codec_context::codec_context(codec codec, stream stream, unsigned threads)
    : codec_handle_(avcodec_alloc_context3(core::get_pointer(codec)), [](pointer p) { avcodec_free_context(&p); })
    , format_stream_(stream)
{
    core::verify(avcodec_parameters_to_context(codec_handle_.get(), format_stream_->codecpar));
    core::verify(av_opt_set_int(codec_handle_.get(), "refcounted_frames", 1, 0));
    core::verify(av_opt_set_int(codec_handle_.get(), "threads", threads, 0));
    core::verify(avcodec_open2(codec_handle_.get(), core::get_pointer(codec), nullptr));
}

media::codec_context::codec_context(format_context& format, media::type media_type, unsigned threads)
{
    auto[codec, stream] = format.demux_with_codec(media_type);
    *this = codec_context{ codec,stream,threads };
}

media::codec_context::pointer media::codec_context::operator->() const
{
    return codec_handle_.get();
}

media::codec_context::operator bool() const
{
    return codec_handle_ != nullptr;
}

bool media::codec_context::valid() const
{
    return !flushed_;
}

int64_t media::codec_context::dispose_count() const
{
    return dispose_count_;
}

int64_t media::codec_context::frame_count() const
{
    return format_stream_->nb_frames;
}

std::vector<media::frame> media::codec_context::decode(packet const& packets) const
{
    if (std::exchange(flushed_, packets.empty()))
        throw std::logic_error{ "prohibit multiple codec context flush" };
    if (format_stream_.index() != packets->stream_index)
        throw std::invalid_argument{ "prohibt decode disparate stream" };
    std::vector<frame> decoded_frames;
    decoded_frames.reserve(packets.empty() ? 10 : 1);
    core::verify(avcodec_send_packet(core::get_pointer(codec_handle_), core::get_pointer(packets)));
    frame current;
    while (avcodec_receive_frame(core::get_pointer(codec_handle_), core::get_pointer(current)) == 0)
        decoded_frames.push_back(std::exchange(current, frame{}));
    dispose_count_ += decoded_frames.size();
    return decoded_frames;
}