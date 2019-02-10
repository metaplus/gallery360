#pragma once
#define __STDC_CONSTANT_MACROS
#pragma warning(push)
#pragma warning(disable:4819)
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#pragma warning(pop)

#include <memory>
#include <string_view>

namespace media
{
    enum class type : int16_t
    {
        audio = AVMEDIA_TYPE_AUDIO,
        video = AVMEDIA_TYPE_VIDEO,
        subtitle = AVMEDIA_TYPE_SUBTITLE,
        unknown = AVMEDIA_TYPE_UNKNOWN,
    };

    class frame final
    {
        using pointer = AVFrame *;
        using reference = AVFrame &;

        struct deleter final
        {
            void operator()(AVFrame* object) const;
        };

        std::unique_ptr<AVFrame, deleter> handle_;

    public:
        frame();
        explicit frame(std::nullptr_t);
        frame(const frame&) = delete;
        frame(frame&&) = default;
        frame& operator=(const frame&) = delete;
        frame& operator=(frame&&) = default;
        ~frame() = default;
        pointer operator->() const;

        bool empty() const;
        void unreference() const;
    };

    class packet final
    {
        using pointer = AVPacket *;
        using reference = AVPacket &;

        struct deleter final
        {
            void operator()(AVPacket* object) const;
        };

        std::unique_ptr<AVPacket, deleter> handle_;

    public:
        packet();
        explicit packet(std::nullptr_t);
        explicit packet(std::basic_string_view<uint8_t> buffer); // copy by av_malloc from buffer view
        explicit packet(std::string_view buffer);
        packet(const packet&) = delete;
        packet(packet&&) = default;
        packet& operator=(const packet&) = delete;
        packet& operator=(packet&&) = default;
        ~packet() = default;
        pointer operator->() const;
        explicit operator bool() const;

        bool empty() const;
        size_t size() const;
        std::basic_string_view<uint8_t> buffer() const;
        void unreference() const;
    };

    static_assert(std::is_nothrow_move_constructible<frame>::value);
    static_assert(std::is_nothrow_move_constructible<packet>::value);
    static_assert(std::is_nothrow_move_assignable<frame>::value);
    static_assert(std::is_nothrow_move_assignable<packet>::value);

    struct codec final : std::reference_wrapper<AVCodec>
    {
        using pointer = type *;
        using reference = type &;
        using parameter = std::reference_wrapper<const AVCodecParameters>;

        codec();
        explicit codec(reference ref);
        explicit codec(pointer ptr);
        pointer operator->() const;
    };

    struct stream final : std::reference_wrapper<AVStream>
    {
        using pointer = type *;
        using reference = type &;
        stream();
        explicit stream(reference ref);
        explicit stream(pointer ptr);
        pointer operator->() const;
        int index() const;
        codec::parameter params() const;
        media::type media() const;
        std::pair<int, int> scale() const;
    };

    struct source final
    {
        struct format final : std::string_view
        {
            using std::string_view::string_view;
            using std::string_view::operator=;
        };

        struct path final : std::string_view
        {
            using std::string_view::string_view;
            using std::string_view::operator=;
        };
    };

    struct sink final
    {
        struct format final : std::string_view
        {
            using std::string_view::string_view;
            using std::string_view::operator=;
        };

        struct path final : std::string_view
        {
            using std::string_view::string_view;
            using std::string_view::operator=;
        };
    };
}
