#include "stdafx.h"
#include "ffmpeg/ffmpeg.h"

av::frame::frame(std::nullptr_t)
    : handle_()
{
}

av::frame::frame()
    : handle_(av_frame_alloc(), [](pointer p) { av_frame_free(&p); })
{
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

av::packet::packet(std::nullptr_t)
    : handle_()
{
}

av::packet::packet(std::basic_string_view<uint8_t> sv)
    : packet()
{
    uint8_t* required_avbuffer = nullptr;
    core::verify(required_avbuffer = static_cast<uint8_t*>(av_malloc(sv.size())));
    std::copy_n(sv.data(), sv.size(), required_avbuffer);
    av_packet_from_data(handle_.get(), required_avbuffer, static_cast<int>(sv.size()));
}

av::packet::packet()
    : handle_(av_packet_alloc(), [](pointer p) { av_packet_free(&p); })
{
}

bool av::packet::empty() const
{
    return handle_ == nullptr || handle_->data == nullptr || handle_->size <= 0;
}

std::basic_string_view<uint8_t> av::packet::buffer_view() const
{
    return { handle_->data,static_cast<size_t>(handle_->size) };
}

std::string_view av::packet::cbuffer_view() const
{
    return {
        reinterpret_cast<char*>(handle_->data),
        static_cast<size_t>(handle_->size)
    };
}

av::packet::pointer av::packet::operator->() const
{
    return handle_.get();
}

void av::packet::unref() const
{
    av_packet_unref(handle_.get());
}

av::stream::stream(reference ref)
    : reference_wrapper(ref)
{
}

av::stream::stream(const pointer ptr)
    : reference_wrapper(*ptr)
{
}

av::codec::codec(reference ref)
    : reference_wrapper(ref)
{
}

av::codec::codec(const pointer ptr)
    : reference_wrapper(*ptr)
{
}

av::codec::pointer av::codec::operator->() const
{
    return std::addressof(get());
}

av::codec::codec()
    : reference_wrapper(core::make_null_reference_wrapper<type>())
{
}

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
{
}

int av::stream::index() const
{
    return get().index;
}
