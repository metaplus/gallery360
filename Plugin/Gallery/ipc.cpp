#include "stdafx.h"
#include "interface.h"
namespace
{
    std::shared_ptr<ipc::channel> channel = nullptr;
    std::future<void> initialize;
}
void dll::ipc_create() {
    initialize = std::async([] {
        channel = std::make_shared<ipc::channel>(true);
    });
}
void dll::ipc_release() {
    initialize.wait();
    channel.reset();
}
std::pair<std::future<ipc::message>, size_t> dll::ipc_async_receive() {
    initialize.wait();
    return channel->async_receive();
}
ipc::message dll::ipc_receive() {
    initialize.wait();
#pragma warning(push)
#pragma warning(disable:4101)
    auto[future, left_count] = channel->async_receive();
#pragma warning(pop)
    return future.get();
}
template<typename Alternate>
void dll::ipc_async_send(Alternate message) {
    static_assert(core::is_within_v<Alternate, ipc::message::is_alternative<Alternate>::value>);
    auto duration = dll::timer_elapsed();
    initialize.wait();
    channel->async_send(std::move(message), std::move(duration));
}
