#include "stdafx.h"
#include "interface.h"
using namespace av;
using namespace core;
enum task { init, parse, decode, last };
namespace
{
    struct frame_queue
    {
        std::deque<av::frame> container;
        mutable std::mutex mutex;
        mutable std::condition_variable condition;
        std::atomic<bool> is_empty;
        frame_queue() = default;
        //std::shared_lock<decltype(mutex)> lock_shared() const { return std::shared_lock<decltype(mutex)>{mutex}; }
        std::unique_lock<decltype(mutex)> lock_exclusive() const { return std::unique_lock<decltype(mutex)>{mutex}; }
    };
    const auto max_fps = 60;
    std::shared_ptr<frame_queue> frames = nullptr;
    namespace routine
    {
        std::shared_future<void> registry; 
        std::shared_future<av::format_context> parse;
        std::shared_future<uint64_t> decode;
    }
    namespace status
    {
        auto& empty = frames->is_empty;
        std::atomic<bool> running = false;
    }
}
auto push_frame = [&](const av::frame& frame)
{
    auto exlock = frames->lock_exclusive();
    if (frames->container.size() > max_fps + 20)
        frames->condition.wait(exlock, [] { return frames->container.size() < max_fps || !status::running.load(std::memory_order_relaxed); });
    if (!status::running.load(std::memory_order_relaxed)) throw core::force_exit_exception{};
    frames->container.push_back(frame);
    frames->is_empty.store(frames->container.empty(), std::memory_order_relaxed);
    exlock.unlock();
    frames->condition.notify_one();
};

auto pop_frame = [&]() -> av::frame
{
    routine::parse.wait();
    auto exlock = frames->lock_exclusive();
    if (frames->container.empty()) 
        frames->condition.wait(exlock, [] { return !frames->container.empty() || !status::running.load(std::memory_order_relaxed); });
    if (!status::running.load(std::memory_order_relaxed)) throw core::force_exit_exception{};
    const auto frame = std::move(frames->container.front());
    frames->container.pop_front();
    const auto size = frames->container.size();
    frames->is_empty.store(size == 0, std::memory_order_relaxed);
    exlock.unlock();
    if (size < max_fps) 
        frames->condition.notify_one();
    return frame;
};
BOOL StoreMediaUrl(LPCSTR url)
{
    try
    {
        const filesystem::path path = url;
        core::verify(is_regular_file(path));
        std::promise<av::format_context> parse;
        routine::parse = parse.get_future().share();
        routine::decode = std::async(std::launch::async, [parse = std::move(parse), path = path.generic_string()]() mutable
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
                reading = !packet.empty();
                if (auto decode_frames = codec.decode(packet); !decode_frames.empty())
                {
                    if (static std::optional<ipc::message> msg; !msg.has_value())
                    {
                        auto msg_time = dll::timer_elapsed();
                        auto msg_body = ipc::message::first_frame_available{};
                        msg.emplace(std::move(msg_body), std::move(msg_time));
                        dll::interprocess_async_send(msg.value());
                    }
                    decode_count += decode_frames.size();
                    std::for_each(decode_frames.begin(), decode_frames.end(), [&](auto& p) { push_frame(p); });
                }
            }
            return decode_count;
        }).share();
    }
    catch (...) { return false; }
    return true;
}
void LoadVideoParams(INT& width, INT& height)
{
    auto format = routine::parse.get();
    auto stream = format.demux<media::video>().first;
    std::tie(width, height) = stream.scale();
}
BOOL IsVideoDrained()
{   
    return routine::decode.wait_for(0ns) == std::future_status::ready && frames->is_empty.load(std::memory_order_acquire);
}
std::optional<av::frame> dll::media_extract_frame()
{
    //if (IsVideoDrained()) return std::nullopt;
    return pop_frame();
}
void dll::media_create()
{
    status::running.store(true, std::memory_order_release);
    frames = std::make_shared<decltype(frames)::element_type>();
    routine::registry = std::async([] { av::register_all(); }).share();     //7ms
}

void dll::media_release()
{
    status::running.store(false, std::memory_order_seq_cst);
    frames->condition.notify_all();
    core::repeat_each([](auto& future) { future.wait(); }, routine::registry, routine::parse, routine::decode);
    frames = nullptr;
}