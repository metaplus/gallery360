#pragma once

namespace core
{
    class time_guard
    {
        std::chrono::steady_clock::time_point time_mark_;
    public:
        time_guard();
        time_guard(const time_guard&) = delete;
        time_guard(time_guard&&) = default;
        time_guard& operator=(const time_guard&) = delete;
        time_guard& operator=(time_guard&&) = default;
        ~time_guard();
    };
    class scope_guard 
    {
        std::function<void()> release_;
    public:
        template<typename Callable>
        explicit scope_guard(Callable callable, bool ctor_invoke = false);
        scope_guard() = default;
        scope_guard(const scope_guard&) = delete;
        scope_guard(scope_guard&&) = default;
        scope_guard& operator=(const scope_guard&) = delete;
        scope_guard& operator=(scope_guard&& other) = default;
        ~scope_guard();
    };
    template <typename Callable>
    scope_guard::scope_guard(Callable callable, const bool ctor_invoke)
        : release_(ctor_invoke ? callable : std::move(callable)) 
    {
        if (ctor_invoke) std::invoke(callable);
    }
    namespace v2
    {
        template<typename Callable>
        class scope_guard : Callable
        {
        public:
            explicit scope_guard(const Callable& c) : Callable(c) {}
            scope_guard() = default;
            scope_guard(const scope_guard&) = delete;
            scope_guard(scope_guard&&) = default;
            scope_guard& operator=(const scope_guard&) = delete;
            scope_guard& operator=(scope_guard&& other) = default;
            ~scope_guard() { std::invoke(*this); }
        };
        template<typename Callable>
        scope_guard<std::decay_t<Callable>> make_scope_guard(Callable&& callable)
        {
            return scope_guard<std::decay_t<Callable>>{ std::forward<Callable>(callable) };
        }
    }
    // Exception guard
}
