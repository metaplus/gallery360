#include "stdafx.h"
#include "export.h"

namespace dll
{
    constexpr size_t media_context::capacity()
    {   // 90 fps + 20 redundancy
        return 90 + 20;
    }

    void media_context::start()
    {
        status_.is_active = true;
        condvar_.notify_all();
    }

    void media_context::stop()
    {
        status_.is_active = false;
        condvar_.notify_all();
    }

    size_t media_context::hash_code() const
    {
        return core::hash_value_from(
            std::string_view{ media_format_->url },
            std::apply(std::multiplies<>{}, resolution()));
    }

    bool media_context::operator<(const context_interface& other) const
    {
        return hash_code() < other.hash_code();
    }

    bool media_context::empty() const
    {
        if (!status_.has_read)
            return false;
        std::lock_guard<std::recursive_mutex> exlock{ rmutex_ };
        return frame_deque_.empty();
    }

    std::pair<int, int> media_context::resolution() const
    {
        return media_format_.demux(media::category::video{}).scale();
    }

    uint64_t media_context::count_frame() const
    {
        return std::max<int64_t>(0, media_format_.demux(media::category::video{})->nb_frames);
    }

    media_context::media_context(const std::string& url)
    {
        media::register_all();
        const_cast<media::format_context&>(media_format_) = media::format_context{ media::source::path{url.data()} };
        const_cast<media::codec_context&>(video_codec_) = std::make_from_tuple<media::codec_context>(
            media_format_.demux_with_codec(media::category::video{}));
        pending_.decode_video = std::async(std::launch::async, [this, decode_count = size_t{ 0 }]() mutable
        {
            //auto& context = dynamic_cast<dll::media_context&>(*self);
            auto& context = *this;
            while (context.status_.is_active && !context.status_.has_read)
            {
                auto packet = context.media_format_.read(media::category::video{});
                context.status_.has_read = packet.empty();
                if (auto decode_frames = context.video_codec_.decode(packet); !decode_frames.empty())
                {
                    decode_count += decode_frames.size();
                    context.push_frames(std::move(decode_frames));
                }
            }
            context.status_.has_decode = true;
            context.condvar_.notify_all();
            return decode_count;
        });
        status_.is_active = true;
    }

    media_context::~media_context()
    {
        media_context::stop();
        if (pending_.read_media.valid()) pending_.read_media.wait();
        if (pending_.decode_video.valid()) pending_.decode_video.wait();
    }

    void media_context::push_frames(std::vector<media::frame>&& frames)
    {
        if (!frames.empty())
        {
            std::unique_lock<std::recursive_mutex> exlock{ rmutex_ };
            if (frame_deque_.size() >= capacity())
                condvar_.wait(exlock, [this] { return frame_deque_.size() < capacity() || !status_.is_active; });
            if (!status_.is_active) throw core::aborted_error{ __PRETTY_FUNCTION__ };
            std::move(frames.begin(), frames.end(), std::back_inserter(frame_deque_));
            exlock.unlock();
        }
        condvar_.notify_one();
    }

    std::optional<media::frame> media_context::pop_frame()
    {
        std::unique_lock<std::recursive_mutex> exlock{ rmutex_ };
        if (frame_deque_.empty())
            condvar_.wait(exlock, [this] { return !frame_deque_.empty() || !status_.is_active || status_.has_decode; });
        if (!status_.is_active)
            throw core::aborted_error{ __PRETTY_FUNCTION__ };
        if (frame_deque_.empty() && status_.has_decode)
            return std::nullopt;
        const auto frame = std::move(frame_deque_.front());
        frame_deque_.pop_front();
        const auto size = frame_deque_.size();
        exlock.unlock();
        if (size < capacity()) condvar_.notify_one();
        return frame;
    }
}