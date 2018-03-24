#include "stdafx.h"
#include "interface.h"

namespace
{
    std::vector<std::shared_ptr<dll::media_session>> media_sessions;
    std::shared_mutex media_smutex;
    std::optional<std::pair<decltype(media_sessions)::iterator, std::shared_lock<std::shared_mutex>>>
        find_session_by_hashid(const size_t hashid)
    {
        std::shared_lock<std::shared_mutex> slock{ media_smutex };
        if (!media_sessions.empty())
        {
            auto iter = std::find_if(media_sessions.begin(), media_sessions.end(),
                [hashid](decltype(media_sessions)::const_reference pss) { return pss->hash_value() == hashid; });
            if (iter != media_sessions.end())
                return std::make_optional(std::make_pair(iter, std::move(slock)));
        }
        return std::nullopt;
    }
}

void unity::_nativeMediaCreate()
{
    av::register_all();
    media_sessions.clear();
}

void unity::_nativeMediaRelease()
{
    std::lock_guard<std::shared_mutex> exlock{ media_smutex };
    for (const auto& session : media_sessions)
    {
        session->pause();
    }
    media_sessions.clear();
}

UINT64 unity::_nativeMediaSessionCreate(LPCSTR url)
{
    const auto session = std::make_shared<dll::media_session>(std::string{ url });
    {
        std::lock_guard<std::shared_mutex> exlock{ media_smutex };
        media_sessions.push_back(session);
    }
    return session->hash_value();
}

void unity::_nativeMediaSessionPause(UINT64 hashID)
{
    const auto result = find_session_by_hashid(hashID);
    if (!result.has_value()) return;
    (*result->first)->pause();
}

void unity::_nativeMediaSessionRelease(UINT64 hashID)
{
    auto result = find_session_by_hashid(hashID);
    if (!result.has_value()) return;
    auto exlock = util::lock_upgrade(result->second);
    *(result->first) = nullptr;
    //std::cerr << "vec size " << media_sessions.size() << "\n";
    media_sessions.erase(result->first);
    //std::cerr << "vec size " << media_sessions.size() << "\n";
}

void unity::_nativeMediaSessionGetResolution(UINT64 hashID, INT& width, INT& height)
{
    const auto result = find_session_by_hashid(hashID);
    if (!result.has_value()) return;
    std::tie(width, height) = (*result->first)->resolution();
}

BOOL unity::_nativeMediaSessionHasNextFrame(UINT64 hashID)
{
    const auto result = find_session_by_hashid(hashID);
    if (!result.has_value()) return false;
    return !(*result->first)->empty();
}

UINT64 unity::debug::_nativeMediaSessionGetFrameCount(UINT64 hashID)
{
    const auto result = find_session_by_hashid(hashID);
    if (!result.has_value()) return 0;
    return  (*result->first)->count_frame();
}

BOOL unity::debug::_nativeMediaSessionDropFrame(UINT64 hashID, UINT64 count)
{
    if (count == 0) return false;
    const auto result = find_session_by_hashid(hashID);
    uint64_t drop_count = 0;
    if (!result.has_value()) return false;
    do
    {
        auto frame = (*result->first)->pop_frame();
        drop_count += frame.has_value();
    } while (--count != 0);
    return drop_count > 0;
}

std::optional<av::frame> dll::media_module::getter::decoded_frame()
{
    std::lock_guard<std::shared_mutex> exlock{ media_smutex };
    if (media_sessions.empty()) return std::nullopt;
    return media_sessions.back()->pop_frame();
}

#ifdef GALLERY_USE_LEGACY 

using namespace av;
using namespace core;

namespace
{
    namespace routine
    {
        std::shared_future<void> registry;
        std::shared_future<av::format_context> parse;
        std::shared_future<uint64_t> decode;
        std::promise<std::pair<std::string, av::codec_context>> retrieve;
        std::vector<std::function<void()>> cleanup;
    }

    namespace status
    {
        std::atomic<bool> available = false;
        std::atomic<bool> running = false;
    }



    struct frame_queue
    {
        std::deque<av::frame> container;
        mutable std::mutex mutex;
        mutable std::condition_variable condition;
        std::atomic<bool> empty = true;
        frame_queue() = default;
    };

    constexpr auto max_fps = 60;
    std::shared_ptr<frame_queue> frames = nullptr;

    const auto push_frames = [](std::vector<av::frame>&& fvec)
    {
        if (fvec.empty()) return;
        std::unique_lock<std::mutex> exlock{ frames->mutex };
        if (frames->container.size() > max_fps + 20)
            frames->condition.wait(exlock, [] { return
                frames->container.size() < max_fps || !status::running.load(std::memory_order_relaxed); });
        if (!status::running.load(std::memory_order_relaxed))
            throw core::aborted_error{};
        std::move(fvec.begin(), fvec.end(), std::back_inserter(frames->container));
        frames->empty.store(frames->container.empty(), std::memory_order_relaxed);
        exlock.unlock();
        frames->condition.notify_one();
    };

    const auto pop_frame = []() -> av::frame
    {
        routine::parse.wait();
        std::unique_lock<std::mutex> exlock{ frames->mutex };
        if (frames->container.empty())
            frames->condition.wait(exlock, [] { return
                !frames->container.empty() || !status::running.load(std::memory_order_relaxed); });
        if (!status::running.load(std::memory_order_relaxed))
            throw core::aborted_error{};
        const auto frame = std::move(frames->container.front());
        frames->container.pop_front();
        const auto size = frames->container.size();
        frames->empty.store(size == 0, std::memory_order_relaxed);
        exlock.unlock();
        if (size < max_fps)
            frames->condition.notify_one();
        return frame;
    };
}

BOOL unity::store_media_url(LPCSTR url)
{
    try
    {
        const filesystem::path path = url;
        core::verify(is_regular_file(path));
        std::promise<av::format_context> parse;
        routine::parse = parse.get_future().share();
        routine::decode = std::async(std::launch::async,
            [parse = std::move(parse), path = path.generic_string()]() mutable
        {
            uint64_t decode_count = 0;
            routine::registry.wait();
            format_context format{ source{path} };
            parse.set_value(format);
            auto[cdc, srm] = format.demux<media::video>();
            codec_context codec{ cdc,srm };
            auto reading = true;
            while (status::running.load(std::memory_order_acquire) && reading)
            {
                auto packet = format.read<media::video>();
                if (packet.empty())
                    reading = false;
                if (auto decode_frames = codec.decode(packet); !decode_frames.empty())
                {
                    if (static std::optional<ipc::message> first_available; !first_available.has_value())
                    {
                        routine::retrieve.set_value(std::make_pair(format->filename, codec));
                        first_available.emplace(ipc::message{}.emplace(ipc::first_frame_available{}));
                        dll::interprocess_async_send(first_available.value());
                        routine::cleanup.emplace_back(
                            []() { if (first_available.has_value()) first_available = std::nullopt; });
                    }
                    decode_count += decode_frames.size();
                    push_frames(std::move(decode_frames));
                    if (!reading)
                        status::available.store(false, std::memory_order_release);
                }
            }
            return decode_count;
        }).share();
    }
    catch (...)
    {
        return false;
    }
    return true;
}

void unity::load_video_params(INT& width, INT& height)
{
    auto format = routine::parse.get();
    auto stream = format.demux<media::video>().second;
    std::tie(width, height) = stream.scale();
}

BOOL unity::is_video_available()
{
    return status::available.load(std::memory_order_acquire) || !frames->empty.load(std::memory_order_relaxed);
}

std::optional<av::frame> dll::media_retrieve_frame()
{
    //if (IsVideoAvailable()) return std::nullopt;
    return pop_frame();
}

std::pair<std::string, av::codec_context> dll::media_retrieve_format()
{
    return routine::retrieve.get_future().get();
}

void dll::media_prepare()
{
    routine::retrieve = {};
}

void dll::media_create()
{
    status::available.store(true, std::memory_order_relaxed);
    status::running.store(true, std::memory_order_release);
    frames = std::make_shared<decltype(frames)::element_type>();
    routine::registry = std::async([] { av::register_all(); }).share();     //7ms
}

void dll::media_release()
{
    status::available.store(false, std::memory_order_relaxed);
    status::running.store(false, std::memory_order_seq_cst);
    frames->condition.notify_all();
    core::repeat_each([](auto& future)
    {
        future.wait();
        future = {};
    }, routine::registry, routine::parse, routine::decode);
    if (!routine::cleanup.empty())
    {
        for (const auto& func : routine::cleanup)
            func();
        routine::cleanup.clear();
    }
    routine::retrieve = {};
    frames = nullptr;
}

#endif  // GALLERY_USE_LEGACY