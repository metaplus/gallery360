#include "stdafx.h"
#include "interface.h"
using namespace av;
using namespace core;
enum task { init, parse, decode, last };
namespace
{
    std::unique_ptr<tbb::concurrent_unordered_map<task, std::shared_future<std::any>>>  pending = nullptr;
    std::unique_ptr<tbb::concurrent_bounded_queue<frame>> frames = nullptr;
    const auto max_fps = 60;
    std::atomic<bool> ongoing = true;
}
auto revocable_wait = [&](auto& future)       //cancelable wait
{
    //using type=decltype(std::declval<decltype(future)>().get());
    if constexpr(core::is_future<decltype(future)>::value) 
    {
        while (future.wait_for(100us) != std::future_status::ready)
        {
            if (!ongoing.load(std::memory_order_acquire))
                throw std::runtime_error{ "foreced quit" };
        }
    }
    else
        static_assert(false, "demand (shared_)future<T> param");
};
auto revocable_push = [&](decltype(frames)::element_type::value_type& elem)
{
    while (!frames->try_push(elem))
    {
        if (!ongoing.load(std::memory_order_acquire))
            throw std::runtime_error{ "forced quit" };
        std::this_thread::sleep_for(50us);
    }
};
auto revocable_pop = [&] {
    decltype(frames)::element_type::value_type data;
    revocable_wait(pending->at(parse)); 
    while (!frames->try_pop(data))
    {
        if (pending->at(decode).wait_for(0ns) == std::future_status::ready)
            return frame{};
        if (!ongoing.load(std::memory_order_acquire))
            throw std::runtime_error{ "forced quit" };
        std::this_thread::sleep_for(50us);
    }
    return data;
};
BOOL ParseMedia(LPCSTR url)
{
    try
    {
        verify(is_regular_file(filesystem::path{ url }));
        (*pending)[parse] = std::async(std::launch::async, [&, path = std::string{ url }]{
            pending->at(init).wait();
            format_context format{ source{path} };
            auto[srm, cdc] = format.demux<media::video>();
            codec_context codec{ cdc,srm };
            (*pending)[decode] = std::async(std::launch::async, [&, format, codec]() mutable {
                revocable_wait(pending->at(parse));
                auto on_read = true;
                while (on_read)
                {
                    auto pkt = format.read<media::video>();
                    on_read = !pkt.empty();
                    if (auto frms = codec.decode(pkt); !frms.empty())
                        std::for_each(frms.begin(), frms.end(), [&](auto& p) { revocable_push(p); });
                }
                return std::make_any<int64_t>(codec.count());
            }).share();
            return std::make_any<format_context>(format);
        }).share();
    }
    catch (...) { return false; }
    return true;
}
void LoadParamsVideo(INT& width, INT& height)
{
    revocable_wait(pending->at(parse));
    auto format = std::any_cast<format_context>(pending->at(parse).get());
    auto stream = format.demux<media::video>().first;
    std::tie(width, height) = stream.scale();
}
BOOL IsDrainedVideo()
{   //swaping first 2 AND operands is accurate but may gain performance penalty, thus add 3rd operand as amendment
    return frames->empty() && pending->at(decode).wait_for(0ns) == std::future_status::ready && frames->empty();
}
std::optional<av::frame> dll::extract_frame()
{
    if (IsDrainedVideo())
        return std::nullopt;
    return revocable_pop();
}
void dll::media_create()
{
    ongoing.store(true, std::memory_order_relaxed);
    frames = std::make_unique<decltype(frames)::element_type>();
    pending = std::make_unique<decltype(pending)::element_type>();
    frames->set_capacity(max_fps + 20);
    (*pending)[init] = std::async([] {
        register_all();     //7ms
        return std::any{};
    }).share();
}
void dll::media_clear()
{
    frames->abort();
    frames->clear();
    pending->clear();
    frames.reset();
    pending.reset();
}
void dll::media_release()
{
    ongoing.store(false, std::memory_order_release);
    for (auto index = init; index != last; core::enum_advance(index, 1))
    {
        if (pending->count(index) != 0 && pending->at(index).wait_for(3s) != std::future_status::ready)
            throw std::runtime_error{ "critical blocking accident" };
    }
    dll::media_clear();
}
