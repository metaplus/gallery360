#include "stdafx.h"
#include "interprocess.h"
#include "interface.h"

namespace
{
    using namespace core::literals;
    namespace config
    {
        constexpr auto identity_monitor = "___MessageQueue$MonitorExe_"sv;
        constexpr auto identity_plugin = "___MessageQueue$PluginDll_"sv;
        constexpr auto shmem_capacity = 512_kbyte;
        //constexpr auto try_interval = 5ms;              // maximum 1s/90fps/2operation
        constexpr auto try_interval = 1s / 90 / 2;
    }
}

constexpr size_t ipc::message::size() 
{
    return size_trait::value;
}

ipc::message::message()
    : data_(), duration_(dll::timer_elapsed()), description_()
{}

const std::chrono::high_resolution_clock::duration& ipc::message::timing() const
{
    return duration_;
}
constexpr size_t ipc::message::index() const 
{
    return data_.index();
}

constexpr size_t ipc::channel::buffer_size() 
{
    return ipc::message::size() * 2;
}

ipc::channel::channel(const bool open_only)
try : running_(true), send_context_(), recv_context_(), prioritizer_(&default_prioritize)
{
    if (open_only)
    {
        send_context_.messages.emplace(interprocess::open_only, config::identity_plugin.data());
        recv_context_.messages.emplace(interprocess::open_only, config::identity_monitor.data());
    }
    else
    {
        constexpr auto msg_size = message::size();
        constexpr auto msg_capcity = config::shmem_capacity / msg_size;
        send_context_.shmem_remover.emplace([] { interprocess::message_queue::remove(config::identity_monitor.data()); }, true);
        recv_context_.shmem_remover.emplace([] { interprocess::message_queue::remove(config::identity_plugin.data()); }, true);
        send_context_.messages.emplace(interprocess::create_only, config::identity_monitor.data(), msg_capcity, msg_size);
        recv_context_.messages.emplace(interprocess::create_only, config::identity_plugin.data(), msg_capcity, msg_size);
        core::verify(send_context_.messages->get_max_msg() > 0, recv_context_.messages->get_max_msg() > 0);
    }
}
catch (...)
{
    running_.store(false, std::memory_order_seq_cst);
    send_context_.pending.abort_and_wait();
    recv_context_.pending.abort_and_wait();
    send_context_.messages = std::nullopt;
    recv_context_.messages = std::nullopt;
    send_context_.shmem_remover = std::nullopt;
    recv_context_.shmem_remover = std::nullopt;
    throw;
}

void ipc::channel::prioritize_by(std::function<unsigned(const ipc::message&)> prior)
{
    prioritizer_.swap(prior);
}

std::future<ipc::message> ipc::channel::async_receive()
{
    if (!valid()) 
        return {};
    return recv_context_.pending.append(
        std::bind(&channel::do_receive, this), sync::use_future);
}

void ipc::channel::async_send(ipc::message message)
{
    if (!valid()) 
        return;
    send_context_.pending.append(
        std::bind(&channel::do_send, this, std::move(message)));
}

void ipc::channel::send(ipc::message message)
{
    if (!valid()) 
        return;
    send_context_.pending.append(
        std::bind(&channel::do_send, this, std::move(message)), sync::use_future).wait();
}

ipc::message ipc::channel::receive()
{
    return async_receive().get();
}

bool ipc::channel::valid() const
{
    return running_.load(std::memory_order_acquire)
        && send_context_.messages.has_value() && recv_context_.messages.has_value();
}

void ipc::channel::wait()
{
    core::repeat_each([](endpoint& context)
    {
        context.pending.wait();
    }, send_context_, recv_context_);
}

ipc::channel::~channel()
{
    running_.store(false);
    core::repeat_each([](endpoint& context)
    {
        if (!context.messages.has_value()) return;
        context.pending.abort_and_wait();
        context.messages.reset();
    }, send_context_, recv_context_);
}

unsigned ipc::channel::default_prioritize(const ipc::message& message)
{
    return message.is<ipc::message::info_launch>() ? std::numeric_limits<unsigned>::max() :
        message.is<ipc::message::info_exit>() ? std::numeric_limits<unsigned>::min() :
        /*1 + */static_cast<unsigned>(ipc::message::index_size() - message.index());
}

void ipc::channel::do_send(const ipc::message& message)
{
    static thread_local std::stringstream stream;
    stream.str(""s); stream.clear();
    {
        cereal::BinaryOutputArchive oarchive{ stream };     //considering static thread_local std::optional<cereal::BinaryOutputArchive>
        oarchive << message;
    }
    auto buffer = stream.str();
    core::verify(buffer.size() < buffer_size());            //assume buffer_size never achieved
    while (running_.load(std::memory_order_acquire))
    {
        if (send_context_.messages->try_send(buffer.data(), buffer.size(), prioritizer_(message)))
            return;
        std::this_thread::sleep_for(config::try_interval);
    }
    throw core::force_exit_exception{};
}

ipc::message ipc::channel::do_receive()
{
    std::string buffer(buffer_size(), 0);
    auto[recv_size, priority] = std::pair<size_t, unsigned>{};
    while (running_.load(std::memory_order_acquire))
    {
        if (!recv_context_.messages->try_receive(buffer.data(), buffer.size(), recv_size, priority)) {
            std::this_thread::sleep_for(config::try_interval);
            continue;
        }
        core::verify(recv_size < buffer_size());            //exception if filled
        buffer.resize(recv_size);
        static thread_local std::stringstream stream;
        stream.clear(); stream.str(std::move(buffer));
        ipc::message message;
        {
            cereal::BinaryInputArchive iarchive{ stream };  //considering static thread_local std::optional<cereal::BinaryInputArchive>
            iarchive >> message;
        }
        return message;
    }
    throw core::force_exit_exception{};
}