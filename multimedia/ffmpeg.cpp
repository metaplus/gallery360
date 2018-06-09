#include "stdafx.h"
#include "ffmpeg.h"

#ifdef MULTIMEDIA_USE_MSGPACK
#include <msgpack.hpp>
#include <msgpack/adaptor/define_decl.hpp>
#endif // MULTIMEDIA_USE_MSGPACK

av::frame::frame()
    : handle_(av_frame_alloc(), [](pointer p) { av_frame_free(&p); })
{}

av::frame::frame(std::nullptr_t)
{}

void av::register_all()
{
    // static std::once_flag once;
    // std::call_once(once, [] { av_register_all(); });
}

bool av::frame::empty() const
{
    return handle_ == nullptr || handle_->data == nullptr || handle_->data[0] == nullptr;
}

av::frame::pointer av::frame::operator->() const
{
    return handle_.get();
}

void av::frame::unref() const
{
    av_frame_unref(handle_.get());
}

#ifdef MULTIMEDIA_USE_MSGPACK
struct av::packet::chunk
{
    explicit chunk(const packet& packet)
        : stream_index(packet->stream_index)
        , is_key_frame(packet->flags == AV_PKT_FLAG_KEY)
        , duration(packet->duration)
        , position(packet->pos)
        , buffer_view(reinterpret_cast<const char*>(packet->data), packet->size)
    {
    }

    int stream_index = 0;
    bool is_key_frame = false;
    int64_t duration = 0;
    int64_t position = 0;
    //std::string_view buffer_view = ""sv;
    msgpack::type::raw_ref buffer_view = {};

    MSGPACK_DEFINE(stream_index, is_key_frame, duration, position, buffer_view)
};
#endif // MULTIMEDIA_USE_MSGPACK

av::packet::packet(std::nullptr_t)
{}

av::packet::packet(std::basic_string_view<uint8_t> sv)
    : packet()
{
    uint8_t* required_avbuffer = nullptr;
    core::verify(required_avbuffer = static_cast<uint8_t*>(av_malloc(sv.size() + AV_INPUT_BUFFER_PADDING_SIZE)));
    std::copy_n(sv.data(), sv.size(), required_avbuffer);
    core::verify(0 == av_packet_from_data(handle_.get(), required_avbuffer, static_cast<int>(sv.size())));
}

av::packet::packet(std::string_view csv)
    : packet(std::basic_string_view<uint8_t>{reinterpret_cast<const uint8_t*>(csv.data()), csv.size()})
{}

av::packet::packet()
    : handle_(av_packet_alloc(), [](pointer p) { av_packet_free(&p); })
{}

bool av::packet::empty() const
{
    return handle_ == nullptr || handle_->data == nullptr || handle_->size <= 0;
}

std::basic_string_view<uint8_t> av::packet::ubufview() const
{
    return { handle_->data,static_cast<size_t>(handle_->size) };
}

std::string_view av::packet::bufview() const
{
    return { reinterpret_cast<char*>(handle_->data),static_cast<size_t>(handle_->size) };
}

#ifdef MULTIMEDIA_USE_MSGPACK
std::string av::packet::serialize() const
{
    msgpack::sbuffer sbuf(handle_->size + sizeof(chunk));
    msgpack::pack(sbuf, chunk{ *this });
    return std::string{ sbuf.data(),sbuf.size() };
}
#endif // MULTIMEDIA_USE_MSGPACK

av::packet::pointer av::packet::operator->() const
{
    return handle_.get();
}

av::packet::operator bool() const
{
    return !empty();
}

void av::packet::unref() const
{
    av_packet_unref(handle_.get());
}

av::stream::stream(reference ref)
    : reference_wrapper(ref)
{}

av::stream::stream(const pointer ptr)
    : reference_wrapper(*ptr)
{}

av::codec::codec(reference ref)
    : reference_wrapper(ref)
{}

av::codec::codec(const pointer ptr)
    : reference_wrapper(*ptr)
{}

av::codec::pointer av::codec::operator->() const
{
    return std::addressof(get());
}

av::codec::codec()
    : reference_wrapper(core::make_null_reference_wrapper<type>())
{}

av::codec::parameter av::stream::params() const
{
    return std::cref(*get().codecpar);
}

av::media::type av::stream::media() const
{
    return get().codecpar->codec_type;
}

std::pair<int, int> av::stream::scale() const
{
    return std::make_pair(get().codecpar->width, get().codecpar->height);
}

av::stream::pointer av::stream::operator->() const
{
    return std::addressof(get());
}

av::stream::stream()
    : reference_wrapper(core::make_null_reference_wrapper<type>())
{}

int av::stream::index() const
{
    return get().index;
}
