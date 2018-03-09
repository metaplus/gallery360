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
namespace impl
{
    template<size_t Index, typename ...Types>
    size_t valid_size(const std::variant<Types...>& var)
    {
        if (std::get_if<Index>(&var) != nullptr)
            return sizeof(std::variant_alternative_t<Index, std::variant<Types...>>);
        if constexpr(Index < sizeof...(Types)-1)
            return impl::valid_size<Index + 1>(var);
        throw std::bad_variant_access{};
    }
}
constexpr size_t ipc::message::size() noexcept
{
    return size_trait::value;
}
size_t ipc::message::valid_size() const noexcept
{
    return impl::valid_size<0>(data_);
}

const std::chrono::high_resolution_clock::duration& ipc::message::timing() const
{
    return duration_;
}
constexpr size_t ipc::message::index() const noexcept 
{
    return data_.index();
}
constexpr size_t ipc::message::aligned_size(const size_t align) noexcept 
{
    return size() + (align - size() % align) % align;
}
constexpr size_t ipc::channel::buffer_size() noexcept 
{
    return ipc::message::size() * 2;
}
ipc::channel::channel(const bool open_only)
try : running_(true), send_context_(), recv_context_() 
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
        send_context_.shmem_remover = core::scope_guard{ [] { interprocess::message_queue::remove(config::identity_monitor.data()); }, true };
        recv_context_.shmem_remover = core::scope_guard{ [] { interprocess::message_queue::remove(config::identity_plugin.data()); }, true };
        send_context_.messages.emplace(interprocess::create_only, config::identity_monitor.data(), msg_capcity, msg_size);
        recv_context_.messages.emplace(interprocess::create_only, config::identity_plugin.data(), msg_capcity, msg_size);
        core::verify(send_context_.messages->get_max_msg() > 0, recv_context_.messages->get_max_msg() > 0);
    }
}
catch (...) 
{
    running_.store(false, std::memory_order_seq_cst);
    send_context_.messages = std::nullopt;
    recv_context_.messages = std::nullopt;
    throw;      
}

std::pair<std::future<ipc::message>, size_t> ipc::channel::async_receive()
{
    if (!valid()) return {};
    std::packaged_task<ipc::message()> recv_task{ [this]() mutable
    {
        std::stringstream stream;
        std::string buffer(buffer_size(), 0);
        auto[recv_size, priority] = std::pair<size_t, unsigned>{};
        while (running_.load(std::memory_order_acquire))
        {
            if (!recv_context_.messages->try_receive(buffer.data(), buffer.size(), recv_size, priority)) {
                std::this_thread::sleep_for(4ms);
                continue;
            }
            core::verify(recv_size < buffer_size());            //exception if filled
            buffer.resize(recv_size);
            stream.clear(); stream.str(std::move(buffer));
            ipc::message message;
            {
                cereal::BinaryInputArchive iarchive{ stream };  //considering static thread_local std::optional<cereal::BinaryInputArchive>
                iarchive >> message;
            }
            return message;
        }
        throw core::force_exit_exception{};
    } };
    auto future = recv_task.get_future();
    recv_context_.pending.append(std::move(recv_task));
    return std::make_pair(std::move(future), recv_context_.messages->get_num_msg());
}
void ipc::channel::async_send(ipc::message message)
{
    if (!valid()) return;
    send_context_.pending.append([this, message = std::move(message)]() mutable
    {
        const auto priority = static_cast<unsigned int>(message.index());
        std::stringstream stream;
        stream.str(""s); stream.clear();
        {
            cereal::BinaryOutputArchive oarchive{ stream };     //considering static thread_local std::optional<cereal::BinaryOutputArchive>
            oarchive << message;
        }
        auto buffer = stream.str();
        core::verify(buffer.size() < buffer_size());            //assume buffer_size never achieved
        while (running_.load(std::memory_order_acquire))
        {
            if (send_context_.messages->try_send(buffer.data(), buffer.size(), priority))
                return;
            std::this_thread::sleep_for(4ms);
        }
        throw core::force_exit_exception{};
    });
}
bool ipc::channel::valid() const 
{
    return running_.load(std::memory_order_acquire) && send_context_.messages.has_value() && recv_context_.messages.has_value();
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
