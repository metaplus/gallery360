#pragma once
namespace core
{
    class force_exit_exception : public std::exception
    {
        std::string_view description_;
    public:
        explicit force_exit_exception(const std::string_view desc = ""sv) 
            : description_(desc) {}
        const char* what() const override
        {
            return description_.empty() ? "force_exit_exception" : description_.data();
        }
    };

    /** reference empirical values for low payload
    *  @par sleep std::this_thread::sleep_for(0ns) 9~20us
    *  @par yield std::this_thread::yield ~3us
    *  @par wait std::future::wait_for(0ns) 6~15us
    */

    namespace revokable
    {
        template<typename Future>                   
        std::enable_if_t<core::is_future<Future>::value>
            wait(Future&& future, std::atomic<bool>& permit, std::chrono::steady_clock::duration interval = 0ns) {
            auto attempt = future.wait_for(0ns);
            if (attempt == std::future_status::deferred)
                throw std::invalid_argument{ "prohibit deferred future, otherwise inevitably infinite blocking potential" };
            while (attempt != std::future_status::ready) {
                if (!permit.load(std::memory_order_acquire))
                    throw core::force_exit_exception{};
                attempt = future.wait_for(interval);
            }
        }
    }
}
