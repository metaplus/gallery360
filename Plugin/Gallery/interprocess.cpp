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
constexpr size_t ipc::channel::buffer_size() noexcept {
    return ipc::message::size() * 3;
}
ipc::channel::channel(const bool open_only)
try : running_(true), send_context_(), recv_context_() {
    if (open_only) {
        send_context_.messages.emplace(interprocess::open_only, config::identity_plugin.data());
        recv_context_.messages.emplace(interprocess::open_only, config::identity_monitor.data());
        const auto lefrt = recv_context_.messages->get_num_msg();
        const auto lefrt2 = send_context_.messages->get_num_msg();
        auto dummy = 1;
    }
    else {
        constexpr auto msg_size = message::size();
        constexpr auto msg_capcity = config::shmem_capacity / msg_size;
        send_context_.shmem_remover = core::scope_guard{ [] { interprocess::message_queue::remove(config::identity_monitor.data()); }, true };
        recv_context_.shmem_remover = core::scope_guard{ [] { interprocess::message_queue::remove(config::identity_plugin.data()); }, true };
        send_context_.messages.emplace(interprocess::create_only, config::identity_monitor.data(), msg_capcity, msg_size);
        recv_context_.messages.emplace(interprocess::create_only, config::identity_plugin.data(), msg_capcity, msg_size);
        core::verify(send_context_.messages->get_max_msg() > 0, recv_context_.messages->get_max_msg() > 0);
    }
    core::repeat_each([this](endpoint& context) {
        auto& running = running_;
        auto& tasks = context.task_queue;
        context.task_worker = std::thread{ [this, &running, &tasks] {
            auto count = -1;
            while (running.load(std::memory_order_acquire)) {
                std::decay_t<decltype(tasks)>::value_type future;
                if (!tasks.try_pop(future)) {
                    std::this_thread::sleep_for(1ms);
                    continue;
                }
                try {
                    auto x = send_context_.messages->get_num_msg();
                    auto y = recv_context_.messages->get_num_msg();
                    future.get();       //if atomic running_ is false, exception throwed here
                    fmt::print(std::cerr, "sending fulfilled {}\n", ++count);
                }
                catch (...) {
                    break;
                }  //exception not propagated outside thread
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
std::pair<std::future<ipc::message>, size_t> ipc::channel::async_receive() {
    std::promise<ipc::message> promise;
    auto future = promise.get_future();
    const auto lefrt = recv_context_.messages->get_num_msg();
    const auto lefrt2 = send_context_.messages->get_num_msg();
    auto task = std::async(std::launch::deferred, 
        [this, promise = std::move(promise)]() mutable {
        static thread_local std::stringstream stream;
        std::string buffer(buffer_size(), 0);
        auto[recv_size, priority] = std::pair<size_t, unsigned>{};
        while(running_.load(std::memory_order_acquire)) {
            const auto lefrt = recv_context_.messages->get_num_msg();
            if (!recv_context_.messages->try_receive(buffer.data(), buffer.size(), recv_size, priority)) {
                std::this_thread::sleep_for(1ms);
                continue;
            }
            core::verify(recv_size < buffer_size());  //exception if filled
            buffer.resize(recv_size);
            stream.clear();
            stream.str(std::move(buffer));
            ipc::message message;
            {
                cereal::BinaryInputArchive iarchive{ stream };
                iarchive >> message;
            }
            promise.set_value(std::move(message));
            return;
        }
        promise.set_exception(std::make_exception_ptr(core::force_exit_exception{}));
        throw core::force_exit_exception{};
    });
    recv_context_.task_queue.push(std::move(task));
    return std::make_pair(std::move(future), recv_context_.messages->get_num_msg());
}
bool ipc::channel::valid() const noexcept {
    return send_context_.messages.has_value() && recv_context_.messages.has_value();
}
ipc::channel::~channel() {
    running_.store(false);
    core::repeat_each([](endpoint& context) {
        context.task_worker.join();
        context.task_queue.clear();
        context.messages.reset();
    }, send_context_, recv_context_);
}