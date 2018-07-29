#include "stdafx.h"


namespace dll::internal
{
    folly::ThreadedExecutor& io_executor()
    {
        static std::optional<folly::ThreadedExecutor> executor;
        static std::once_flag flag;
        std::call_once(flag, []
                       {
                           executor.emplace(dll::create_thread_factory("MediaContext"));
                       });
        return executor.value();
    }
}

namespace dll
{
    media_context::media_context(std::string url)
        : url_(url)
        , decode_executor_(folly::SerialExecutor::create(folly::getKeepAliveToken(dll::cpu_executor())))
    {
        internal::io_executor().add([this]
                                    {
                                        std::invoke(on_parse_read_loop(), std::string_view{ url_ });
                                    });
    }

    folly::Function<void(std::string_view)> media_context::on_parse_read_loop()
    {
        return [this](std::string_view url)
        {
            media_format_ = media::format_context{ media::source::path{ url.data() } };
            video_codec_ = media::codec_context{ media_format_, media::type::video };
            parse_complete_.post();
            media::packet packet = media_format_.read(media::type::video);
            try
            {
                while (!packet.empty())
                {
                    if (stop_request_.load())
                    {
                        throw std::runtime_error{ __PRETTY_FUNCTION__ };
                    }
                    frames_.write(wait_queue_vacancy(std::move(packet)));
                    ++read_step_count_;
                    packet = media_format_.read(media::type::video);
                }
                frames_.write(wait_queue_vacancy(std::move(packet)));
            }
            catch (std::runtime_error&)
            {
                fmt::print("media_context stopped\n");
                pop_decode_frame();
                frames_.write(wait_queue_vacancy(media::packet{}));;
            }
            read_complete_.post();
        };
    }

    folly::Function<media_context::container<media::frame>()> media_context::on_decode_frame_container(media::packet&& packet)
    {
        return[this, packet = std::move(packet)]
        {
            if (packet.empty())
            {
                decode_complete_.post();
                return container<media::frame>{};
            }
            auto frames = video_codec_.decode(packet);
            decode_step_count_ += frames.size();
            container<media::frame> decode_frames;
            decode_frames.assign(std::make_move_iterator(frames.begin()), std::make_move_iterator(frames.end()));
            return decode_frames;
        };
    }

    folly::Future<media_context::container<media::frame>> media_context::wait_queue_vacancy(media::packet&& packet)
    {
        if (frames_.isFull())
        {
            read_token_.wait();
            read_token_.reset();
        }
        return folly::via(decode_executor_.get(), on_decode_frame_container(std::move(packet)));
    }

    std::optional<media::frame> media_context::pop_decode_frame()
    {
        folly::Future<container<media::frame>>* front_ptr = frames_.frontPtr();
        if (front_ptr != nullptr && front_ptr->valid() && front_ptr->isReady())
        {
            container<media::frame> front_frames = front_ptr->get();
            if (front_frames.size())
            {
                media::frame frame = std::move(front_frames.front());
                if (front_frames.size() > 1)
                    front_frames.erase(front_frames.begin());
                else
                    pop_frame_and_resume_read();
                return frame;
            }
            pop_frame_and_resume_read();
        }
        return std::nullopt;
    }

    void media_context::pop_frame_and_resume_read()
    {
        bool const is_queue_full = frames_.isFull();
        frames_.popFront();
        if (is_queue_full)
            read_token_.post();
    }

    std::pair<int64_t, int64_t> media_context::stop_and_wait()
    {
        stop_request_.store(true);
        pop_decode_frame();
        read_complete_.wait();
        decode_complete_.wait();
        return { read_step_count_, decode_step_count_ };
    }

    void media_context::wait_parse_complete() const
    {
        parse_complete_.wait();
    }

    void media_context::wait_decode_complete() const
    {
        decode_complete_.wait();
    }

    bool media_context::is_decode_complete() const
    {
        return frames_.isEmpty() && decode_complete_.ready();
    }


    std::pair<int, int> media_context::resolution() const
    {
        return media_format_.demux(media::type::video).scale();
    }

    bool media_context::operator<(media_context const& that) const
    {
        return ordinal_ < that.ordinal_;
    }
}
