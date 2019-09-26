#include "stdafx.h"
#include "context.h"
#include "core/core.h"
#include "core/verify.hpp"

extern "C" {
#include <libavutil/opt.h>
}

namespace media
{
    using namespace detail;

    void io_context::deleter::operator()(pointer context) const {
        av_freep(&context->buffer);
        av_freep(&context);
    }

    io_context::io_context(std::unique_ptr<io_base> io)
        : io_base_{ std::move(io) }
        , io_handle_{
              avio_alloc_context(
                  static_cast<uint8_t*>(av_malloc(default_cache_page_size)),
                  default_cache_page_size, default_buffer_writable,
                  io_base_.get(),
                  io_base_->readable() ? on_read_buffer : nullptr,
                  io_base_->writable() ? on_write_buffer : nullptr,
                  io_base_->seekable() ? on_seek_stream : nullptr),
              deleter{}
          } {
        assert(io_base_ != nullptr);
        assert(io_handle_ != nullptr);
    }

    io_context::io_context(read_context&& read, write_context&& write, seek_context&& seek)
        : io_context{
            std::make_unique<generic_cursor>(
                std::move(read), std::move(write), std::move(seek))
        } {}

    io_context::pointer io_context::operator->() const {
        return io_handle_.get();
    }

    io_context::operator bool() const {
        return io_handle_ != nullptr && io_base_ != nullptr;
    }

    int io_context::on_read_buffer(void* opaque, uint8_t* buffer, int size) {
        return static_cast<io_base*>(opaque)->read(buffer, size);
    }

    int io_context::on_write_buffer(void* opaque, uint8_t* buffer, int size) {
        return static_cast<io_base*>(opaque)->write(buffer, size);
    }

    int64_t io_context::on_seek_stream(void* opaque, int64_t offset, int whence) {
        return static_cast<io_base*>(opaque)->seek(offset, whence);
    }

    void format_context::deleter::operator()(pointer context) const {
        avformat_close_input(&context);
    }

    format_context::format_context(io_context& io, source::format iformat)
        : io_handle_(io) {
        auto format = avformat_alloc_context();
        format->pb = io_handle_.get().operator->();
        [[maybe_unused]] const auto success =
            avformat_open_input(&format, nullptr,
                                iformat.empty() ? nullptr : av_find_input_format(iformat.data()), nullptr);
        assert(success == 0);
        assert(format != nullptr);
        format_handle_ = { format, deleter{} };
        core::verify(avformat_find_stream_info(format, nullptr));
    }

    format_context::format_context(io_context& io, sink::format oformat)
        : io_handle_{ io } {
        core::not_implemented_error::throw_directly();
    }

    format_context::format_context(source::path ipath)
        : io_handle_{ core::make_null_reference_wrapper<io_context>() } {
        pointer format = nullptr;
        [[maybe_unused]] const auto success = avformat_open_input(&format, ipath.data(), nullptr, nullptr);
        assert(success == 0);
        format_handle_ = { format, deleter{} };
        core::verify(avformat_find_stream_info(format, nullptr));
#ifdef _DEBUG
        av_dump_format(format, 0, format->url, 0);
#endif
    }

    format_context::format_context(sink::path opath)
        : io_handle_{ core::make_null_reference_wrapper<io_context>() } {
        core::not_implemented_error::throw_directly();
    }

    format_context::pointer format_context::operator->() const {
        return format_handle_.get();
    }

    bool io_context::available() const noexcept {
        return io_base_ != nullptr && io_base_->available();
    }

    format_context::operator bool() const {
        return format_handle_ != nullptr;
    }

    stream format_context::demux(type media_type) const {
        return stream{
            format_handle_->streams[
                av_find_best_stream(format_handle_.get(),
                                    static_cast<AVMediaType>(media_type),
                                    -1, -1, nullptr, 0)]
        };
    }

    std::pair<codec, stream> format_context::demux_with_codec(type media_type) const {
        codec::pointer cdc = nullptr;
        const auto format_ptr = format_handle_.get();
        const auto index = av_find_best_stream(format_ptr, static_cast<AVMediaType>(media_type),
                                               -1, -1, &cdc, 0);
        if (index >= 0) {
            return std::make_pair(codec{ cdc },
                                  stream{ format_ptr->streams[index] });
        }
        return std::make_pair(codec{ cdc },
                              stream{ format_ptr->streams[0] });
    }

    packet format_context::read(type media_type) const {
        packet packet;
        auto read_result = 0;
        while ((read_result = av_read_frame(
                format_handle_.get(), core::get_pointer(packet))) == 0
            && media_type != type::unknown
            && !core::underlying_same(
                media_type, format_handle_
                            ->streams[packet->stream_index]
                            ->codecpar->codec_type)) {
            packet.unreference();
        }
        return packet;
    }

    std::vector<packet> format_context::read(const size_t count, type media_type) const {
        std::vector<packet> packets;
        packets.reserve(count);
        std::generate_n(std::back_inserter(packets), count,
                        [this, media_type] {
                            return read(media_type);
                        });
        return packets;
    }

    void codec_context::deleter::operator()(pointer context) const {
        avcodec_free_context(&context);
    }

    codec_context::codec_context(codec codec, stream stream, unsigned threads)
        : codec_handle_{
              avcodec_alloc_context3(core::get_pointer(codec)),
              deleter{}
          }
        , format_stream_(stream) {
        core::verify(avcodec_parameters_to_context(codec_handle_.get(), format_stream_->codecpar));
        core::verify(av_opt_set_int(codec_handle_.get(), "refcounted_frames", 1, 0));
        core::verify(av_opt_set_int(codec_handle_.get(), "threads", threads, 0));
        core::verify(avcodec_open2(codec_handle_.get(), core::get_pointer(codec), nullptr));
    }

    codec_context::codec_context(format_context& format, type media_type, unsigned threads) {
        auto [codec, stream] = format.demux_with_codec(media_type);
        *this = codec_context{ codec, stream, threads };
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
        const auto flushed = std::exchange(flushed_, packets.empty());
        assert(!flushed);
        assert(format_stream_.index() == packets->stream_index);
        detail::vector<frame> full_frames;
        if constexpr (std::is_same<decltype(full_frames), std::vector<frame>>::value) {
            full_frames.reserve(packets.empty() ? 10 : 1);
        }
        auto decode_start_time = absl::Now();
        core::verify(avcodec_send_packet(core::get_pointer(codec_handle_),
                                         core::get_pointer(packets)));
        frame temp_frame;
        while (0 == avcodec_receive_frame(core::get_pointer(codec_handle_),
                                          core::get_pointer(temp_frame))) {
            temp_frame.process_duration(absl::Now() - decode_start_time);
            full_frames.push_back(std::exchange(temp_frame, frame{}));
            decode_start_time = absl::Now();
        }
        dispose_count_ += full_frames.size();
        return full_frames;
    }
}
