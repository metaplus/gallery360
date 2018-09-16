#include "stdafx.h"
#include "context.h"

namespace media
{
    using namespace detail;

    io_context::io_context(std::shared_ptr<io_base> io)
        : io_base_(std::move(io))
        , io_handle_(
            avio_alloc_context(
                static_cast<uint8_t*>(av_malloc(default_cache_page_size)),
                default_cache_page_size, default_buffer_writable, &io_base_, //io_base_.get(),
                io_base_->readable() ? on_read_buffer : nullptr,
                io_base_->writable() ? on_write_buffer : nullptr,
                io_base_->seekable() ? on_seek_stream : nullptr),
            [](pointer ptr) { av_freep(&ptr->buffer);  av_freep(&ptr); }) {
        assert(io_base_ != nullptr);
        assert(io_handle_ != nullptr);
    }

    io_context::io_context(read_context&& read, write_context&& write, seek_context&& seek)
        : io_context(std::make_shared<generic_cursor>(std::move(read), std::move(write), std::move(seek))) {}

    io_context::pointer io_context::operator->() const {
        return io_handle_.get();
    }

    io_context::operator bool() const {
        return io_handle_ != nullptr && io_base_ != nullptr;
    }

    int io_context::on_read_buffer(void* opaque, uint8_t* buffer, int size) {
        return static_cast<std::shared_ptr<io_base>*>(opaque)->get()->read(buffer, size);
    }

    int io_context::on_write_buffer(void* opaque, uint8_t* buffer, int size) {
        return static_cast<std::shared_ptr<io_base>*>(opaque)->get()->write(buffer, size);
    }

    int64_t io_context::on_seek_stream(void* opaque, int64_t offset, int whence) {
        return static_cast<std::shared_ptr<io_base>*>(opaque)->get()->seek(offset, whence);
    }

#ifdef MULTIMEDIA_USE_LEGACY
    format_context::format_context(std::variant<source, sink> io)
        : format_handle_(nullptr)
        , io_handle_() {
        std::visit([this](auto&& arg) constexpr
        {
            register_all();
            if constexpr (std::is_same_v<source, std::decay_t<decltype(arg)>>) {
                pointer ptr = nullptr;
                core::verify(avformat_open_input(&ptr, arg.url.c_str(), nullptr, nullptr));
                format_handle_.reset(ptr, [](pointer p) { avformat_close_input(&p); });
                core::verify(avformat_find_stream_info(ptr, nullptr));   // 60ms+
            #ifdef _DEBUG
                av_dump_format(ptr, 0, ptr->filename, 0);
            #endif // define _DEBUG
            } else {
                // TODO: SINK BRANCH
                throw core::not_implemented_error{};
            }
        }, io);
    }
#endif // define MULTIMEDIA_USE_LEGACY

    format_context::format_context(io_context io, source::format iformat)
        : format_handle_(nullptr)
        , io_handle_(std::move(io)) {
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

    format_context::format_context(io_context io, sink::format oformat) {
        throw core::not_implemented_error{ "format_context::constructor" };
    }

    format_context::format_context(io_context io, bool source) {
        *this = source ?
            format_context{ std::move(io), source::format{""} } :
            format_context{ std::move(io), sink::format{""} };
    }

    format_context::format_context(source::path ipath)
        : format_handle_(nullptr) {
        pointer ptr = nullptr;
        core::verify(avformat_open_input(&ptr, ipath.data(), nullptr, nullptr));
        format_handle_.reset(ptr, [](pointer p) { avformat_close_input(&p); });
        core::verify(avformat_find_stream_info(ptr, nullptr));   // 60ms+
    #ifdef _DEBUG
        av_dump_format(ptr, 0, ptr->url, 0);
    #endif
    }

    format_context::format_context(sink::path opath) {
        throw core::not_implemented_error{ "not_implemented_error" };
    }

    format_context::pointer format_context::operator->() const {
        return format_handle_.get();
    }

    bool io_context::available() const noexcept {
        return io_base_ != nullptr && io_base_->available();
    }

    std::shared_ptr<io_base> io_context::exchange_cursor(std::shared_ptr<io_base> cursor) {
        return std::exchange(io_base_, cursor);
    }

    format_context::operator bool() const {
        return format_handle_ != nullptr;
    }

    stream format_context::demux(type media_type) const {
        return stream{ format_handle_->streams[
            av_find_best_stream(format_handle_.get(), static_cast<AVMediaType>(media_type), -1, -1, nullptr, 0)] };
    }

    std::pair<codec, stream> format_context::demux_with_codec(type media_type) const {
        codec::pointer cdc = nullptr;
        const auto format_ptr = format_handle_.get();
        const auto index = av_find_best_stream(format_ptr, static_cast<AVMediaType>(media_type), -1, -1, &cdc, 0);
        return std::make_pair(codec{ cdc }, stream{ format_ptr->streams[index] });
    }

    packet format_context::read(type media_type) const {
        packet packet;
        auto read_result = 0;
        while ((read_result = av_read_frame(format_handle_.get(), core::get_pointer(packet))) == 0
               && media_type != type::unknown
               && !core::underlying_same(media_type, format_handle_->streams[packet->stream_index]->codecpar->codec_type)) {
            packet.unref();
        }
        return packet;
    }

    std::vector<packet> format_context::read(const size_t count, type media_type) const {
        std::vector<packet> packets; packets.reserve(count);
        std::generate_n(std::back_inserter(packets), count,
                        [this, media_type] { return read(media_type); });
        return packets;
    }

    codec_context::codec_context(codec codec, stream stream, unsigned threads)
        : codec_handle_(avcodec_alloc_context3(core::get_pointer(codec)), [](pointer p) { avcodec_free_context(&p); })
        , format_stream_(stream) {
        core::verify(avcodec_parameters_to_context(codec_handle_.get(), format_stream_->codecpar));
        core::verify(av_opt_set_int(codec_handle_.get(), "refcounted_frames", 1, 0));
        core::verify(av_opt_set_int(codec_handle_.get(), "threads", threads, 0));
        core::verify(avcodec_open2(codec_handle_.get(), core::get_pointer(codec), nullptr));
    }

    codec_context::codec_context(format_context& format, type media_type, unsigned threads) {
        auto[codec, stream] = format.demux_with_codec(media_type);
        *this = codec_context{ codec,stream,threads };
    }

    codec_context::pointer codec_context::operator->() const {
        return codec_handle_.get();
    }

    codec_context::operator bool() const {
        return codec_handle_ != nullptr;
    }

    bool codec_context::valid() const {
        return !flushed_;
    }

    int64_t codec_context::dispose_count() const {
        return dispose_count_;
    }

    int64_t codec_context::frame_count() const {
        return format_stream_->nb_frames;
    }

    detail::vector<frame> codec_context::decode(packet const& packets) const {
        assert(!std::exchange(flushed_, packets.empty()));
        assert(format_stream_.index() == packets->stream_index);
        detail::vector<frame> full_frames;
        if constexpr (std::is_same<decltype(full_frames), std::vector<frame>>::value) {
            full_frames.reserve(packets.empty() ? 10 : 1);
        }
        core::verify(avcodec_send_packet(core::get_pointer(codec_handle_), core::get_pointer(packets)));
        frame temp_frame;
        while (avcodec_receive_frame(core::get_pointer(codec_handle_), core::get_pointer(temp_frame)) == 0) {
            full_frames.push_back(std::exchange(temp_frame, frame{}));
        }
        dispose_count_ += full_frames.size();
        return full_frames;
    }
}