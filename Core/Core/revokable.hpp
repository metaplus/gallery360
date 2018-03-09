#pragma once
namespace core
{
    class force_exit_exception : public std::exception
    {
        std::string_view description_;
    public:
        explicit force_exit_exception(const std::string_view desc = ""sv) 
            : description_(desc)
        {}
        const char* what() const override
        {
            return description_.empty() ? "force_exit_exception" : description_.data();
        }
    };
    /** reference empirical values for low payload
    *  @par sleep std::this_thread::sleep_for(0ns) 9~20us
    *  @par yield std::this_thread::yield 3~7us
    *  @par wait std::future::wait_for(0ns) 6~15us
    *  @par wait std::future::wait $if-closure-completed$ 1us
    */
    namespace revokable
    {
        template<typename Future>                   
        std::enable_if_t<meta::is_future<Future>::value>
            wait(Future&& future, std::atomic<bool>& permit, std::chrono::steady_clock::duration interval = 0ns) 
        {
            auto attempt = future.wait_for(0ns);
            if (attempt == std::future_status::deferred) 
                throw std::invalid_argument{ "prohibit deferred future, otherwise inevitably suffers infinite blocking potential" };
            while (attempt != std::future_status::ready) 
            {
                if (!permit.load(std::memory_order_acquire)) throw core::force_exit_exception{};
                attempt = future.wait_for(interval);
            }
        }
        template<typename Callable>
        std::enable_if_t<std::is_invocable_v<Callable>>
            wait(Callable callable, std::atomic<bool>& permit, std::chrono::steady_clock::duration interval = 0ns)
        {
            auto future = std::async(std::move(callable));
            while (future.wait_for(interval) != std::future_status::ready)
                if (!permit.load(std::memory_order_acquire)) throw core::force_exit_exception{};
        }
    }
}