#include "stdafx.h"

namespace dll
{
    media_context_optimal::media_context_optimal(std::string url)
        : read_executor_(folly::SerialExecutor::create(folly::getKeepAliveToken(dll::cpu_executor())))
        , decode_executor_(folly::SerialExecutor::create(folly::getKeepAliveToken(dll::cpu_executor())))
    {
        frames_.write(url_.getSemiFuture()
                          .via(&dll::cpu_executor()).then(on_parse_format())
                          .via(read_executor_.get()).then(on_read_packet())
                          .via(decode_executor_.get()).then(on_decode_frame()));
        url_.setValue(url);
    }

    folly::Function<void(std::string_view)> media_context_optimal::on_parse_format()
    {
        return [this](std::string_view url)
        {
            media_format_ = media::format_context{ media::source::path{ url.data() } };
            video_codec_ = media::codec_context{ media_format_, media::category::video{} };
            parse_complete_.post();
        };
    }

    folly::Function<media::packet()> media_context_optimal::on_read_packet()
    {
        return [this]
        {
            fmt::print("reading {}\n", read_step_count_);
            auto packet = media_format_.read(media::category::video{});
            ++read_step_count_;
            if (packet.empty())
                read_complete_.post();
            else if (!frames_.isFull())
                frames_.write(chain_media_process_stage());
            return packet;
        };
    }

    folly::Function<media_context_optimal::frame_container(media::packet)> media_context_optimal::on_decode_frame()
    {
        return [this](media::packet packet)
        {
            fmt::print("decoding {} exist {}\n", decode_step_count_, !packet.empty());
            ++decode_step_count_;
            if (packet.empty())
            {
                fmt::print("----- decode finish -----\n");
                if (!decode_complete_.ready())
                    decode_complete_.post();
                return frame_container{};
            }
            auto frames = video_codec_.decode(packet);
            fmt::print("decode frameSize {}\n", frames.size());
            frame_container decode_frames;
            decode_frames.assign(std::make_move_iterator(frames.begin()), std::make_move_iterator(frames.end()));
            return decode_frames;
        };
    }

    folly::Future<media_context_optimal::frame_container> media_context_optimal::chain_media_process_stage()
    {
        return folly::via(read_executor_.get(), on_read_packet())
            .then(decode_executor_.get(), on_decode_frame());
    }

    std::optional<media::frame> media_context_optimal::pop_decode_frame()
    {
        folly::Future<frame_container>* front_ptr = frames_.frontPtr();
        if (front_ptr != nullptr && front_ptr->valid() && front_ptr->isReady())
        {
            frame_container front_frames = front_ptr->get();
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

    void media_context_optimal::pop_frame_and_resume_read()
    {
        frames_.popFront();
        if (frames_.isEmpty() && !read_complete_.ready())
            frames_.write(chain_media_process_stage());
    }


    void media_context_optimal::wait_parser_complete() const
    {
        parse_complete_.wait();
    }

    void media_context_optimal::wait_decode_complete() const
    {
        decode_complete_.wait();
    }

    bool media_context_optimal::is_decode_complete() const
    {
        return decode_complete_.ready();
    }


    std::pair<int, int> media_context_optimal::resolution() const
    {
        return media_format_.demux(media::category::video{}).scale();
    }

    bool media_context_optimal::operator<(media_context_optimal const& that) const
    {
        return ordinal_ < that.ordinal_;
    }
}
