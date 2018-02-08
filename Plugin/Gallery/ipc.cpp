#include "stdafx.h"
#include "interface.h"
namespace
{
    std::shared_ptr<ipc::channel> channel = nullptr;
    std::unique_ptr<tbb::concurrent_queue<std::packaged_task<void()>>> pending = nullptr;
    std::future<void> initialized;
    std::future<size_t> finished;
}
void dll::ipc_create() {
    //initialized = std::async([] {
    //    channel = std::make_shared<ipc::channel>(true);
    //});
    pending = std::make_unique<decltype(pending)::element_type>();
    pending->emplace([] { channel = std::make_shared<ipc::channel>(true); });
    std::promise<void> promise_initialized;
    initialized = promise_initialized.get_future();
    finished = std::async([initialized = std::move(promise_initialized)]() mutable {
        std::packaged_task<void()> task;
        size_t send_count = 0;
        std::this_thread::sleep_for(300ms);
        while (true)
        {
            if (!pending->try_pop(task))
            {
                std::this_thread::sleep_for(8ms);
                continue;
            }
            if (!task.valid())
                return send_count;
            try
            {
                if (std::invoke(task); ++send_count == 1)
                    initialized.set_value();
            }
            catch (...)
            {
                initialized.set_exception(std::current_exception());
                return send_count;
            }
            //task.reset();
        }
    });
}
void dll::ipc_release() {
    pending->emplace();
    initialized.wait();
    finished.wait();
    channel.reset();
}
void dll::ipc_async_send(ipc::message&& message)
{
    pending->emplace([message = std::move(message)]() { channel->async_send(message); });
}
std::pair<std::future<ipc::message>, size_t> dll::ipc_async_receive() {
    initialized.wait();
    return channel->async_receive();
}
ipc::message dll::ipc_receive() {
    initialized.wait();
#pragma warning(push)
#pragma warning(disable:4101)
    auto[future, left_count] = channel->async_receive();
#pragma warning(pop)
    return future.get();
}
