#include "stdafx.h"
#include "interface.h"

namespace
{
    std::shared_ptr<ipc::channel> channel = nullptr;
    std::future<void> initial;
}

void dll::interprocess_create() {
    initial = std::async([]()
    {
        auto url = dll::media_wait_decoding_start();
        try
        {
            channel = std::make_shared<ipc::channel>(true);
            channel->send(ipc::message{}.emplace(ipc::message::info_url{ std::move(url) }));
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
        if (!channel)
            return temp_mvec.container.clear();
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
