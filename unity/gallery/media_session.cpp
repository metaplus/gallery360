#include "stdafx.h"
#include "interface.h"

namespace dll
{
    constexpr size_t media_session::capacity()
    {   // 90 fps + 20 redundancy
        return 90 + 20;
    }

    void media_session::start()
    {
        status_.is_active = true;
        condvar_.notify_all();
    }

    void media_session::stop()
    {
        status_.is_active = false;
        condvar_.notify_all();
    }

    size_t media_session::hash_value() const
    {
        return core::hash_value(std::string_view{ media_format_->filename },
            std::apply(std::multiplies<>{}, resolution()));
    }

    bool media_session::operator<(const session& other) const
    {
        return hash_value() < other.hash_value();
    }

    bool media_session::empty() const
    {
        if (!status_.has_read)
            return false;
        std::lock_guard<std::recursive_mutex> exlock{ rmutex_ };
        return frame_deque_.empty();
    }

    std::pair<int, int> media_session::resolution() const
    {
        return media_format_.demux(av::media::video{}).scale();
    }

    uint64_t media_session::count_frame() const
    {
        return std::max<int64_t>(0, media_format_.demux(av::media::video{})->nb_frames);
    }

    media_session::media_session(const std::string& url)
    {
        av::register_all();
        const_cast<av::format_context&>(media_format_) = av::format_context{ av::source::path{url} };
        const_cast<av::codec_context&>(video_codec_) = std::make_from_tuple<av::codec_context>(media_format_.demux_with_codec(av::media::video{}));
        action_.video_decoding = std::async(std::launch::async,
            [/*self = shared_from_this()*/this, decode_count = size_t{ 0 }]() mutable
        {
            //auto& session = dynamic_cast<dll::media_session&>(*self);
            auto& session = *this;
            while (session.status_.is_active && !session.status_.has_read)
            {
                auto packet = session.media_format_.read(av::media::video{});
                session.status_.has_read = packet.empty();
                if (auto decode_frames = session.video_codec_.decode(packet); !decode_frames.empty())
                {
                    decode_count += decode_frames.size();
                    std::cout << "decode count " << decode_count << "\n";
                    session.push_frames(std::move(decode_frames));
                }
            }
            session.status_.has_decode = true;
            session.condvar_.notify_all();
            return decode_count;
        });
        status_.is_active = true;
    }

    media_session::~media_session()
    {
        media_session::stop();
        core::repeat_each([](auto& future)
        {
            if (future.valid()) future.wait();
        }, action_.media_reading, action_.video_decoding);
    }

    void media_session::push_frames(std::vector<av::frame>&& frames)
    {
        if (!frames.empty())
        {
            std::unique_lock<std::recursive_mutex> exlock{ rmutex_ };
            if (frame_deque_.size() >= capacity())
                condvar_.wait(exlock, [this] { return frame_deque_.size() < capacity() || !status_.is_active; });
            if (!status_.is_active)
                throw core::aborted_error{};
            std::move(frames.begin(), frames.end(), std::back_inserter(frame_deque_));
            exlock.unlock();
        }
        condvar_.notify_one();
    }

    std::optional<av::frame> media_session::pop_frame()
    {
        std::unique_lock<std::recursive_mutex> exlock{ rmutex_ };
        if (frame_deque_.empty())
            condvar_.wait(exlock, [this] { return !frame_deque_.empty() || !status_.is_active || status_.has_decode; });
        if (!status_.is_active)
            throw core::aborted_error{};
        if (frame_deque_.empty() && status_.has_decode)
            return std::nullopt;
        const auto frame = std::move(frame_deque_.front());
        frame_deque_.pop_front();
        const auto size = frame_deque_.size();
        exlock.unlock();
        if (size < capacity())
            condvar_.notify_one();
        return frame;
    }
}