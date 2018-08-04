#include "stdafx.h"
#include "context.h"

namespace
{
    struct io_context_impl final : media::io_context::io_base
    {
        media::io_context::read_context read_context;
        media::io_context::write_context write_context;
        media::io_context::seek_context seek_context;

        explicit io_context_impl(
            media::io_context::read_context&& rfunc = nullptr,
            media::io_context::write_context&& wfunc = nullptr,
            media::io_context::seek_context&& sfunc = nullptr)
            : read_context(std::move(rfunc))
            , write_context(std::move(wfunc))
            , seek_context(std::move(sfunc))
        {}
        int read(uint8_t* buffer, const int size) override final
        {
            return readable() ? read_context(buffer, size) : std::numeric_limits<int>::min();
        }
        int write(uint8_t* buffer, const int size) override final
        {
            return writable() ? write_context(buffer, size) : std::numeric_limits<int>::min();
        }
        int64_t seek(const int64_t offset, const int whence) override final
        {
            return seekable() ? seek_context(offset, whence) : std::numeric_limits<int64_t>::min();
        }
        bool readable() override final { return read_context != nullptr; }
        bool writable() override final { return write_context != nullptr; }
        bool seekable() override final { return seek_context != nullptr; }
    };

    struct io_curser_impl final : media::io_context::io_base, media::io_context::cursor
    {
        explicit io_curser_impl(buffer_type const& buffer)
            : cursor(buffer) {}
        ~io_curser_impl() override = default;
        int read(uint8_t* buffer, int expect_size) override
        {
            if (buffer_iter != buffer_end)
            {
                auto const read_ptr = static_cast<char const*>((*buffer_iter).data());
                auto const read_size = std::min<int64_t>(expect_size, buffer_size() - buffer_offset);
                std::copy_n(read_ptr + buffer_offset, read_size, buffer);
                buffer_offset += read_size;
                sequence_offset += read_size;
                if (buffer_offset == buffer_size())
                {
                    buffer_iter.operator++();
                    buffer_offset = 0;
                }
                assert(read_size != 0);
                return boost::numeric_cast<int>(read_size);
            }
            return AVERROR_EOF;
        }
        int write(uint8_t* buffer, int size) override
        {
            throw core::not_implemented_error{ "io_curser_impl::write" };
        }
        int64_t seek(int64_t seek_offset, int whence) override
        {
            switch (whence)
            {
                case SEEK_SET:
                fmt::print("SEEK_SET OFFSET {}\n", seek_offset);
                break;
                case SEEK_END:
                fmt::print("SEEK_END OFFSET {}\n", seek_offset);
                seek_offset += sequence_size();
                break;
                case SEEK_CUR:
                fmt::print("SEEK_CUR OFFSET {}\n", seek_offset);
                seek_offset += sequence_offset;
                break;
                case AVSEEK_SIZE:
                fmt::print("AVSEEK_SIZE OFFSET {}\n", seek_offset);
                return sequence_size();
                default:
                throw core::unreachable_execution_branch{ "io_context.seek functor" };
            }
            return seek_sequence(seek_offset);
        }
        bool readable() override { return true; }
        bool writable() override { return false; }
        bool seekable() override { return true; }
    };
}

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
    : io_context(std::make_shared<io_curser_impl>(buffer))
{}

media::io_context::io_context(read_context&& read, write_context&& write, seek_context&& seek)
    : io_context(std::make_shared<io_context_impl>(std::move(read), std::move(write), std::move(seek)))
{}

media::io_context::pointer media::io_context::operator->() const
{
    return io_handle_.get();
}

media::io_context::operator bool() const
{
    return io_handle_ != nullptr && io_base_ != nullptr;
}

media::io_context::cursor::cursor(buffer_type const & buffer)
    : buffer_begin(boost::asio::buffer_sequence_begin(buffer.data()))
    , buffer_end(boost::asio::buffer_sequence_end(buffer.data()))
    , buffer_iter(buffer_begin)
{
    auto& buffer_sizes = core::as_mutable(this->buffer_sizes);
    std::transform(buffer_begin, buffer_end, std::back_inserter(buffer_sizes),
                   [](const_iterator::reference buffer) { return boost::asio::buffer_size(buffer); });
    std::partial_sum(buffer_sizes.begin(), buffer_sizes.end(), buffer_sizes.begin());
}

int64_t media::io_context::cursor::seek_sequence(int64_t seek_offset)
{
    auto const size_iter = std::upper_bound(buffer_sizes.crbegin(), buffer_sizes.crend(), seek_offset, std::greater<>{});
    buffer_iter = std::prev(buffer_end, std::distance(buffer_sizes.crbegin(), size_iter));
    auto const partial_sequence_size = size_iter != buffer_sizes.crend() ? *size_iter : 0;
    buffer_offset = std::min(seek_offset - partial_sequence_size, buffer_size());
    sequence_offset = partial_sequence_size + buffer_offset;
    return sequence_offset;
}

int64_t media::io_context::cursor::buffer_size() const
{
    return boost::numeric_cast<int64_t>(boost::asio::buffer_size(*buffer_iter));
}

int64_t media::io_context::cursor::sequence_size() const
{
    return buffer_sizes.back();
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
           &&  !core::underlying_same(media_type,format_handle_->streams[packet->stream_index]->codecpar->codec_type) )
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