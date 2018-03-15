#include "stdafx.h"
#include "interface.h"

namespace
{
    std::shared_ptr<ipc::channel> channel = nullptr;
    std::future<void> initial;
}

void dll::interprocess_create() {
    initial = std::async(std::launch::async, []()
    {
        const auto[url, codec] = dll::media_retrieve_format();
        try
        {
            channel = std::make_shared<ipc::channel>(true);
            std::map<std::string, std::string> mformat;
            mformat["url"] = url;
            mformat["codec_name"] = codec->codec->long_name;
            mformat["resolution"] = std::to_string(codec->width) + 'x' + std::to_string(codec->height);
            mformat["gop_size"] = std::to_string(codec->gop_size);
            mformat["pixel_format"] = av_get_pix_fmt_name(codec->pix_fmt);
            mformat["frames_count"] = std::to_string(codec.frame_count());
            channel->send(ipc::message{}.emplace(ipc::media_format{ std::move(mformat) }));
        }
        catch (...)
        {
            channel = nullptr;
        }
    });
}

void dll::interprocess_release() {
    if (initial.valid())
        initial.get();
    channel = nullptr;
}

void dll::interprocess_async_send(ipc::message message)
{
    static struct
    {
        std::mutex mutex;
        std::vector<ipc::message> container;
    }temp_mvec;
    static thread_local std::vector<ipc::message> local_mvec;
    if (initial.wait_for(0ns) != std::future_status::ready)
    {
        std::lock_guard<std::mutex> exlock{ temp_mvec.mutex };
        return temp_mvec.container.push_back(std::move(message));
    }
    {
        std::lock_guard<std::mutex> exlock{ temp_mvec.mutex };
        if (!channel) {
            if (!temp_mvec.container.empty())
                temp_mvec.container.clear();
            return;
        }
        if (!temp_mvec.container.empty())
            std::swap(local_mvec, temp_mvec.container);
    }
    if (!local_mvec.empty())
    {
        for (auto& msg : local_mvec)
            channel->async_send(std::move(msg));
        local_mvec.clear();
    }
    channel->async_send(std::move(message));
}

void dll::interprocess_send(ipc::message message)
{
    if (initial.wait(); channel == nullptr)
        return;
    channel->send(std::move(message));
}