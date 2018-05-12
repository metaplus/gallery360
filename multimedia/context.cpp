#include "stdafx.h"
#include "context.h"

av::io_context::io_context(func_tuple&& io_functions, const uint32_t buf_size, const bool buf_writable)
    : io_interface_(make_io_interface(std::move(io_functions)))
    , io_handle_(avio_alloc_context(static_cast<uint8_t*>(av_malloc(buf_size)),
        buf_size, buf_writable, io_interface_.get(),
        io_interface_->readable() ? read_func_delegate : nullptr,
        io_interface_->writable() ? write_func_delegate : nullptr,
        io_interface_->seekable() ? seek_func_delegate : nullptr),
        [](pointer ptr) { av_freep(&ptr->buffer);  av_freep(&ptr); })
{
    std::cerr << "constructor\n";
}

av::io_context::pointer av::io_context::operator->() const
{
    return io_handle_.get();
}

av::io_context::operator bool() const
{
    return io_handle_ != nullptr && io_interface_ != nullptr;
}

std::shared_ptr<av::io_context::io_interface> av::io_context::make_io_interface(func_tuple&& io_functions)
{
    struct io_interface_impl : io_interface
    {
        explicit io_interface_impl(
            std::function<int(uint8_t*, int)>&& rfunc = nullptr,
            std::function<int(uint8_t*, int)>&& wfunc = nullptr,
            std::function<int64_t(int64_t, int)>&& sfunc = nullptr)
            : read_func(std::move(rfunc))
            , write_func(std::move(wfunc))
            , seek_func(std::move(sfunc))
        {}
        int read(uint8_t* buffer, const int size) override final
        {
            return readable() ? read_func(buffer, size) : std::numeric_limits<int>::min();
        }
        int write(uint8_t* buffer, const int size) override final
        {
            return writable() ? write_func(buffer, size) : std::numeric_limits<int>::min();
        }
        int64_t seek(const int64_t offset, const int whence) override final
        {
            return seekable() ? seek_func(offset, whence) : std::numeric_limits<int64_t>::min();
        }
        bool readable() override final { return read_func != nullptr; }
        bool writable() override final { return write_func != nullptr; }
        bool seekable() override final { return seek_func != nullptr; }

        std::function<int(uint8_t*, int)> read_func;
        std::function<int(uint8_t*, int)> write_func;
        std::function<int64_t(int64_t, int)> seek_func;
    };
    return std::make_shared<io_interface_impl>(
        std::move(std::get<0>(io_functions)), std::move(std::get<1>(io_functions)), std::move(std::get<2>(io_functions)));
}

int av::io_context::read_func_delegate(void* opaque, uint8_t* buffer, int size)
{
    return static_cast<io_interface*>(opaque)->read(buffer, size);
}

int av::io_context::write_func_delegate(void* opaque, uint8_t* buffer, int size)
{
    return static_cast<io_interface*>(opaque)->write(buffer, size);
}

int64_t av::io_context::seek_func_delegate(void* opaque, int64_t offset, int whence)
{
    return static_cast<io_interface*>(opaque)->seek(offset, whence);
}

#ifdef MULTIMEDIA_USE_LEGACY
av::format_context::format_context(std::variant<source, sink> io)
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
        }
        else
        {
            // TODO: SINK BRANCH
            throw core::not_implemented_error{};
        }
    }, io);
}
#endif // define MULTIMEDIA_USE_LEGACY

av::format_context::format_context(io_context io, source::format iformat)
    : format_handle_(nullptr)
    , io_handle_(std::move(io))
{
    register_all();
    auto format_ptr = avformat_alloc_context();
    format_ptr->pb = core::get_pointer(io_handle_);
    core::verify(avformat_open_input(&format_ptr, nullptr, av_find_input_format(iformat.data()), nullptr));
    format_handle_.reset(format_ptr, [](pointer p) { avformat_close_input(&p); });
    core::verify(avformat_find_stream_info(format_ptr, nullptr));
#ifdef _DEBUG
    av_dump_format(format_ptr, 0, format_ptr->filename, 0);
#endif
}

av::format_context::format_context(io_context io, sink::format iformat)
{
    throw core::not_implemented_error{};
}

av::format_context::format_context(source::path ipath)
    : format_handle_(nullptr)
{
    register_all();

    pointer ptr = nullptr;
    core::verify(avformat_open_input(&ptr, ipath.data(), nullptr, nullptr));
    format_handle_.reset(ptr, [](pointer p) { avformat_close_input(&p); });
    core::verify(avformat_find_stream_info(ptr, nullptr));   // 60ms+
#ifdef _DEBUG
    av_dump_format(ptr, 0, ptr->filename, 0);
#endif
}

av::format_context::format_context(sink::path opath)
{
    throw core::not_implemented_error{};
}

av::format_context::pointer av::format_context::operator->() const
{
    return format_handle_.get();
}

av::format_context::operator bool() const
{
    return format_handle_ != nullptr;
}

av::stream av::format_context::demux(const media::type media_type) const
{
    return stream{ format_handle_->streams[av_find_best_stream(format_handle_.get(), media_type, -1, -1, nullptr, 0)] };
}

std::pair<av::codec, av::stream> av::format_context::demux_with_codec(const media::type media_type) const
{
    codec::pointer cdc = nullptr;
    const auto format_ptr = format_handle_.get();
    const auto index = av_find_best_stream(format_ptr, media_type, -1, -1, &cdc, 0);
    return std::make_pair(codec{ cdc }, stream{ format_ptr->streams[index] });
}

av::packet av::format_context::read(const std::optional<media::type> media_type) const
{
    packet pkt;
    while (av_read_frame(format_handle_.get(), core::get_pointer(pkt)) == 0
        && media_type.has_value()
        && format_handle_->streams[pkt->stream_index]->codecpar->codec_type != media_type)
    {
        pkt.unref();
    }
    return pkt;
}

std::vector<av::packet> av::format_context::read(const size_t count, std::optional<media::type> media_type) const
{
    std::vector<packet> packets; packets.reserve(count);
    std::generate_n(std::back_inserter(packets), count, [this, media_type] { return read(media_type); });
    return packets;
}

av::codec_context::codec_context(codec codec, const stream stream, const unsigned threads)
    : codec_handle_(avcodec_alloc_context3(core::get_pointer(codec)), [](pointer p) { avcodec_free_context(&p); })
    , stream_(stream)
{
    core::verify(avcodec_parameters_to_context(codec_handle_.get(), stream_->codecpar));
    core::verify(av_opt_set_int(codec_handle_.get(), "refcounted_frames", 1, 0));
    core::verify(av_opt_set_int(codec_handle_.get(), "threads", threads, 0));
    core::verify(avcodec_open2(codec_handle_.get(), core::get_pointer(codec), nullptr));
}

av::codec_context::pointer av::codec_context::operator->() const
{
    return codec_handle_.get();
}

av::codec_context::operator bool() const
{
    return codec_handle_ != nullptr;
}

bool av::codec_context::valid() const
{
    return !status_.flushed;
}

int64_t av::codec_context::decoded_count() const
{
    return status_.count;
}

int64_t av::codec_context::frame_count() const
{
    return stream_->nb_frames;
}

std::vector<av::frame> av::codec_context::decode(const packet& packets) const
{
    if (std::exchange(status_.flushed, packets.empty()))
        throw std::logic_error{ "prohibit multiple codec context flush" };
    if (stream_.index() != packets->stream_index)
        throw std::invalid_argument{ "prohibt decode disparate stream" };
    std::vector<frame> decoded_frames;
    decoded_frames.reserve(packets.empty() ? 10 : 1);
    core::verify(avcodec_send_packet(core::get_pointer(codec_handle_), core::get_pointer(packets)));
    frame current;
    while (avcodec_receive_frame(core::get_pointer(codec_handle_), core::get_pointer(current)) == 0)
        decoded_frames.push_back(std::exchange(current, frame{}));
    status_.count += decoded_frames.size();
    return decoded_frames;
}