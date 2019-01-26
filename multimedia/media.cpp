#include "stdafx.h"
#include "media.h"

void media::frame::deleter::operator()(AVFrame* object) const {
    if (object != nullptr) {
        av_frame_free(&object);
    }
}

media::frame::frame()
    : handle_(av_frame_alloc(), deleter{}) {}

media::frame::frame(std::nullptr_t)
    : handle_(nullptr) {}

bool media::frame::empty() const {
    return handle_ == nullptr || handle_->data == nullptr || handle_->data[0] == nullptr;
}

media::frame::pointer media::frame::operator->() const {
    return handle_.get();
}

void media::frame::unreference() const {
    av_frame_unref(handle_.get());
}

void media::packet::deleter::operator()(AVPacket* object) const {
    if (object != nullptr) {
        av_packet_free(&object);
    }
}

media::packet::packet(std::nullptr_t) {}

media::packet::packet(std::basic_string_view<uint8_t> buffer)
    : packet() {
    uint8_t* required_avbuffer = nullptr;
    core::verify(required_avbuffer = static_cast<uint8_t*>(av_malloc(buffer.size() + AV_INPUT_BUFFER_PADDING_SIZE)));
    std::copy_n(buffer.data(), buffer.size(), required_avbuffer);
    core::verify(0 == av_packet_from_data(handle_.get(), required_avbuffer, static_cast<int>(buffer.size())));
}

media::packet::packet(std::string_view buffer)
    : packet(std::basic_string_view<uint8_t>{ reinterpret_cast<uint8_t const*>(buffer.data()), buffer.size() }) {}

media::packet::packet()
    : handle_(av_packet_alloc(), deleter{}) {}

bool media::packet::empty() const {
    return handle_ == nullptr || handle_->data == nullptr || handle_->size <= 0;
}

size_t media::packet::size() const {
    return handle_ ? handle_->size : 0;
}

std::basic_string_view<uint8_t> media::packet::buffer() const {
    return {
        handle_->data,
        folly::to<size_t>(handle_->size)
    };
}

#ifdef MULTIMEDIA_USE_MSGPACK
std::string media::packet::serialize() const {
    msgpack::sbuffer sbuf(handle_->size + sizeof(chunk));
    msgpack::pack(sbuf, chunk{ *this });
    return std::string{ sbuf.data(),sbuf.size() };
}
#endif // MULTIMEDIA_USE_MSGPACK

media::packet::pointer media::packet::operator->() const {
    return handle_.get();
}

media::packet::operator bool() const {
    return !empty();
}

void media::packet::unreference() const {
    av_packet_unref(handle_.get());
}

media::stream::stream(reference ref)
    : reference_wrapper(ref) {}

media::stream::stream(const pointer ptr)
    : reference_wrapper(*ptr) {}

media::codec::codec(reference ref)
    : reference_wrapper(ref) {}

media::codec::codec(const pointer ptr)
    : reference_wrapper(*ptr) {}

media::codec::pointer media::codec::operator->() const {
    return std::addressof(get());
}

media::codec::codec()
    : reference_wrapper(core::make_null_reference_wrapper<type>()) {}

media::codec::parameter media::stream::params() const {
    return std::cref(*get().codecpar);
}

media::type media::stream::media() const {
    return static_cast<media::type>(get().codecpar->codec_type);
}

std::pair<int, int> media::stream::scale() const {
    return std::make_pair(get().codecpar->width,
                          get().codecpar->height);
}

media::stream::pointer media::stream::operator->() const {
    return std::addressof(get());
}

media::stream::stream()
    : reference_wrapper(core::make_null_reference_wrapper<type>()) {}

int media::stream::index() const {
    return get().index;
}
