#include "stdafx.h"
#include "interface.h"
namespace
{
    std::shared_ptr<ipc::channel> channel = nullptr;
    std::shared_future<void> initial;
}
void dll::interprocess_create() {
    initial = std::async([]() 
    {
        try { channel = std::make_shared<ipc::channel>(true); }
        catch (...) { channel = nullptr; }
    });

}
void dll::interprocess_release() {
    if (initial.valid()) initial.get(); 
    channel.reset();
    //channel = nullptr;
}
void dll::interprocess_async_send(ipc::message message)
{
    static struct
    {
        std::mutex mutex;
        std::vector<ipc::message> container;
    }temporary;
    if (initial.wait_for(0ns) != std::future_status::ready)
    {
        std::lock_guard<std::mutex> exlock{ temporary.mutex };
        temporary.container.push_back(std::move(message));
        return;
    }
    if (!channel || !channel->valid())
    {
        std::lock_guard<std::mutex> exlock{ temporary.mutex };
        if(!temporary.container.empty())
            temporary.container.clear();
        return;
    }
    {
        std::lock_guard<std::mutex> exlock{ temporary.mutex };
        if (!temporary.container.empty())
        {
            for (auto& msg : temporary.container)
                channel->async_send(std::move(msg));
            temporary.container.clear();
        }
    }
    channel->async_send(std::move(message));
}
std::pair<std::future<ipc::message>, size_t> dll::interprocess_async_receive() {
    if (initial.wait_for(0ns) != std::future_status::ready || !channel || !channel->valid()) return {};
    return channel->async_receive();
}