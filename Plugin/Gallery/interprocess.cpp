#include "stdafx.h"
#include "interprocess.h"
namespace
{
    using namespace core::literals;
    namespace config
    {
        constexpr auto identity_monitor = "___MessageQueue$MonitorExe_"sv;
        constexpr auto identity_plugin = "___MessageQueue$PluginDll_"sv;
        constexpr auto shmem_capacity = 512_kbyte;
    }
}
constexpr size_t ipc::message::size() noexcept {
    return size_trait::value;
}
constexpr size_t ipc::message::index() const noexcept {
    return data_.index();
}
constexpr size_t ipc::message::aligned_size(const size_t align) noexcept {
    return size() + (align - size() % align) % align;
}
ipc::channel::channel(std::chrono::steady_clock::duration timing, bool open_only)
try : running_(true), send_context_(), recv_context_() {
    if (open_only) {
        send_context_.messages.emplace(interprocess::open_only, config::identity_plugin.data());
        recv_context_.messages.emplace(interprocess::open_only, config::identity_monitor.data());
    }
    else {
        constexpr auto msg_size = message::size();
        constexpr auto msg_capcity = config::shmem_capacity / msg_size;
        send_context_.shmem_remover = core::scope_guard{ [] { interprocess::message_queue::remove(config::identity_monitor.data()); }, true };
        recv_context_.shmem_remover = core::scope_guard{ [] { interprocess::message_queue::remove(config::identity_plugin.data()); }, true };
        send_context_.messages.emplace(interprocess::create_only, config::identity_monitor.data(), msg_capcity, msg_size);
        recv_context_.messages.emplace(interprocess::create_only, config::identity_plugin.data(), msg_capcity, msg_size);
    }
    core::repeat_each([this](endpoint& context) {
        auto& running = running_;
        auto& tasks = context.task_queue;
        context.task_worker = std::thread{ [&running, &tasks] {
            while (running.load(std::memory_order_acquire)) {
                std::packaged_task<void()> task;
                if (!tasks.try_pop(task)) {
                    std::this_thread::sleep_for(1ms);
                    continue;
                }
                std::invoke(task);
                std::this_thread::yield();
            }
        } };
    }, send_context_, recv_context_);
}
catch (...) {
    running_.store(false, std::memory_order_relaxed);
    send_context_.messages.reset();
    recv_context_.messages.reset();
    throw;
}
bool ipc::channel::valid() const noexcept {
    return send_context_.messages.has_value() && recv_context_.messages.has_value();
}
ipc::channel::~channel() {
    running_.store(false);
    core::repeat_each([](endpoint& context){
        context.task_worker.join();
        context.task_queue.clear();
        context.messages.reset(); 
    }, send_context_, recv_context_);
}


