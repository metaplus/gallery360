#pragma once
namespace core
{
    class force_exit_exception : public std::exception
    {
        std::string_view description_;
    public:
        explicit force_exit_exception(const std::string_view desc = ""sv) noexcept
            : description_(desc) {}
        const char* what() const override
        {
            return description_.empty() ? "force_exit_exception" : description_.data();
        }
    };
    namespace revokable
    {
        template<typename Future>
        std::enable_if_t<core::is_future<std::decay_t<Future>>::value>
            wait(Future&& future,std::atomic<bool>& permit,std::chrono::microseconds interval)
        {
            if (auto test = future.wait_for(0ns); test == std::future_status::deferred)
                throw std::invalid_argument{ "prohibit deferred (shared_)future" };
            else if (test != std::future_status::ready)
            {
                while (future.wait_for(interval) != std::future_status::ready)
                {
                    if (!permit.load(std::memory_order_acquire))
                        throw core::force_exit_exception{};
                }
            }
        }
        template<typename Future>
        std::enable_if_t<core::is_future<std::decay_t<Future>>::value>
            yield_wait(Future&& future, std::atomic<bool>& permit)
        {
            if (auto test = future.wait_for(0ns); test == std::future_status::deferred)
                throw std::invalid_argument{ "prohibit deferred (shared_)future" };
            else if (test != std::future_status::ready)
            {
                while (future.wait_for(0ns) != std::future_status::ready)
                {
                    if (!permit.load(std::memory_order_acquire))
                        throw core::force_exit_exception{};
                    std::this_thread::yield();
                }
            }
        }
    }
}


static_assert(std::is_nothrow_destructible_v<core::force_exit_exception>);