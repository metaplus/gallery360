#pragma once

namespace media::component
{
    using boost::beast::multi_buffer;
    using boost::asio::const_buffer;

    class frame_segmentor
    {
        struct impl;
        std::shared_ptr<impl> impl_;

        struct buffer_drained_exception;

    public:
        frame_segmentor() = default;
        frame_segmentor(const frame_segmentor&) = default;
        frame_segmentor(frame_segmentor&&) noexcept = default;
        frame_segmentor& operator=(const frame_segmentor&) = default;
        frame_segmentor& operator=(frame_segmentor&&) noexcept = default;
        ~frame_segmentor() = default;

        void parse_context(std::list<const_buffer> buffer_list);
        bool context_valid() const noexcept;
        bool buffer_available() const;
        void reset_buffer(multi_buffer&& buffer);
        bool consume_one();
    };

}