#include "stdafx.h"
#include "interface.h"
using namespace av;
using namespace core;

namespace
{
    namespace routine
    {
        std::shared_future<void> registry;
        std::shared_future<av::format_context> parse;
        std::shared_future<uint64_t> decode;
        std::promise<std::string> urlstr;
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
            throw core::force_exit_exception{};
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
            throw core::force_exit_exception{};
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

BOOL unity::StoreMediaUrl(LPCSTR url)
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
            auto[srm, cdc] = format.demux<media::video>();
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
                        routine::urlstr.set_value(routine::parse.get()->filename);
                        first_available.emplace(ipc::message{}.emplace(ipc::first_frame_available{}));
                        dll::interprocess_async_send(first_available.value());
                        routine::cleanup.emplace_back(
                            []() { if (first_available.has_value()) first_available = std::nullopt; });
                    }
                    decode_count += decode_frames.size();
                    push_frames(std::move(decode_frames));
                    if(!reading)
                        status::available.store(false, std::memory_order_release);
                }
            }
            return decode_count;
        }).share();
    }
    catch (...) { return false; }
    return true;
}

void unity::LoadVideoParams(INT& width, INT& height)
{
    auto format = routine::parse.get();
    auto stream = format.demux<media::video>().first;
    std::tie(width, height) = stream.scale();
}

BOOL unity::IsVideoAvailable()
{
    return status::available.load(std::memory_order_acquire) || !frames->empty.load(std::memory_order_relaxed);
}

std::optional<av::frame> dll::media_extract_frame()
{
    //if (IsVideoAvailable()) return std::nullopt;
    return pop_frame();
}

std::string dll::media_wait_decoding_start()
{
    return routine::urlstr.get_future().get();
}

void dll::media_prepare()
{
    routine::urlstr = {};
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
    routine::urlstr = {};
    frames = nullptr;
}