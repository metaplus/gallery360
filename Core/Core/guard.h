#pragma once

namespace core
{
    class time_guard : boost::noncopyable
    {
        std::chrono::steady_clock::time_point time_mark_;
    public:
        time_guard();
        ~time_guard();
    };
    class scope_guard : boost::noncopyable
    {
        std::function<void()> release_;
    public:
        template<typename Callable>
        explicit scope_guard(Callable&& c);
        ~scope_guard() noexcept(false);
    };
    template <typename Callable>
    scope_guard::scope_guard(Callable&& c)
        :release_(std::forward<Callable>(c))
    {
    }
}
