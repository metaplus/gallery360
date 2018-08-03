#include "stdafx.h"
#include "export.h"

namespace
{
    int16_t thread_count_per_codec = 0;
}

namespace unity
{
    INT16 _nativeConfigureMedia(INT16 streams)
    {
        thread_count_per_codec = std::max<int16_t>(2, streams / std::thread::hardware_concurrency());
        return thread_count_per_codec;
    }
}

namespace dll
{
    player_context::player_context(folly::Uri uri, ordinal ordinal)
        : uri_(std::move(uri))
        , ordinal_(ordinal)
        , future_net_client_(net_module::establish_http_session(uri_))
    {
        boost::promise<media::format_context> promise_parsed;
        future_parsed_ = promise_parsed.get_future();
        thread_ = std::thread{ on_media_streaming(std::move(promise_parsed)) };
    }

    player_context::~player_context()
    {
        if (thread_.joinable())
            thread_.join();
    }

    folly::Function<void()> player_context::on_media_streaming(boost::promise<media::format_context>&& promise_parsed)
    {
        return[this, promise_parsed = std::move(promise_parsed)]() mutable
        {
            auto request = net::make_http_request<request_body>(uri_.host(), uri_.path());
            boost::promise<response_container> promise_response;
            auto net_client = future_net_client_.get();
            auto future_response = net_client->async_send_request(std::move(request));
            while (true)
            {
                auto response = future_response.get();
                auto respones_reason = response.reason();
                assert(respones_reason == "OK");
                media::io_context media_io{ response.body() };
                media::format_context media_format{ media_io,true };
                frame_amount_ = media_format.demux(media::type::video)->nb_frames;
                assert(frame_amount_ > 0);
                media::codec_context video_codec{ media_format, media::type::video, boost::numeric_cast<unsigned>(thread_count_per_codec) };
                promise_parsed.set_value(media_format);
                media::packet packet = media_format.read(media::type::video);
                try
                {
                    while (!packet.empty())
                    {
                        auto decoded_frames = video_codec.decode(packet);
                        for (auto& frame : decoded_frames)
                        {
                            if (!std::atomic_load(&active_))
                                throw std::runtime_error{ "abort throw" };
                            frames_.add(std::move(frame));
                        }
                        packet = media_format.read(media::type::video);
                    }
                    frames_.add(media::frame{ nullptr });
                }
                catch (...)
                {
                    fmt::print("abort catch");
                }
                break;
            }
            on_decode_complete_.post();
        };
    }

    std::pair<int, int> player_context::resolution()
    {
        auto media_format = future_parsed_.get();
        return media_format.demux(media::type::video).scale();
    }

    media::frame player_context::take_decode_frame()
    {
        if (!on_last_frame_)
        {
            auto frame = frames_.take();
            on_last_frame_ = frame.empty();
            return frame;
        }
        return media::frame{ nullptr };
    }

    uint64_t player_context::available_size()
    {
        return frames_.size();
    }

    void player_context::deactive()
    {
        std::atomic_store(&active_, false);
        frames_.try_take_for(50ms);
    }

    void player_context::deactive_and_wait()
    {
        deactive();
        on_decode_complete_.wait();
    }

    bool player_context::is_last_frame_taken() const
    {
        return on_last_frame_;
    }
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
            frames->condition.wait(exlock, []
                                   {
                                       return
                                           frames->container.size() < max_fps || !status::running.load(std::memory_order_relaxed);
                                   });
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
            frames->condition.wait(exlock, []
                                   {
                                       return
                                           !frames->container.empty() || !status::running.load(std::memory_order_relaxed);
                                   });
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
            auto[cdc, srm] = format.demux_with_codec<media::video>();
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
    auto stream = format.demux_with_codec<media::video>().second;
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
