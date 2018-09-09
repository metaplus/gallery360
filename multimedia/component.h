#pragma once

namespace media::component
{
    using boost::beast::multi_buffer;
    using frame_consumer = folly::Function<void()>;
    using frame_consumer_builder = folly::Function<frame_consumer(std::vector<multi_buffer>)>;

    class media_manager
    {
        struct impl;
        std::shared_ptr<impl> impl_;

        struct buffer_drained_exception;

    public:

        void reset_buffer(multi_buffer& buffer);
    };

}