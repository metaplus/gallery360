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
        explicit scope_guard(Callable callable, bool ctor_invoke = false) noexcept(std::is_nothrow_invocable_v<Callable>);
        ~scope_guard() noexcept(false);
    };
    template <typename Callable>
    scope_guard::scope_guard(Callable callable, const bool ctor_invoke) noexcept(std::is_nothrow_invocable_v<Callable>)
        :release_(ctor_invoke ? callable : std::move(callable))
    {
        if (ctor_invoke) std::invoke(callable);
    }
}
