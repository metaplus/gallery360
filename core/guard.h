#pragma once

namespace core
{
    class time_guard
    {
    public:
        time_guard();
        time_guard(const time_guard&) = delete;
        time_guard(time_guard&&) = default;
        time_guard& operator=(const time_guard&) = delete;
        time_guard& operator=(time_guard&&) = default;
        ~time_guard();
    private:
        std::chrono::steady_clock::time_point time_mark_;
    };

    class scope_guard 
    {
    public:
        explicit scope_guard(std::function<void()> release, bool ctor_invoke = false);
        scope_guard() = default;
        scope_guard(const scope_guard&) = delete;
        scope_guard(scope_guard&&) = default;
        scope_guard& operator=(const scope_guard&) = delete;
        scope_guard& operator=(scope_guard&& other) = default;
        ~scope_guard();
    private:
        std::function<void()> release_;
    };

    template<typename Callable>
    class scope_guard_generic : Callable
    {
    public:
        explicit scope_guard_generic(Callable&& callable);
        scope_guard_generic() = default;
        scope_guard_generic(const scope_guard_generic&) = delete;
        scope_guard_generic(scope_guard_generic&&) = default;
        scope_guard_generic& operator=(const scope_guard_generic&) = delete;
        scope_guard_generic& operator=(scope_guard_generic&&) = default;
        ~scope_guard_generic();
    private:
        using Callable::operator();
    };

    template <typename Callable>
    scope_guard_generic<Callable>::scope_guard_generic(Callable&& callable)
        : Callable(std::forward<Callable>(callable))
    {
    }

    template <typename Callable>
    scope_guard_generic<Callable>::~scope_guard_generic()
    {
        (*this)();
    }

    template<typename Callable>
    scope_guard_generic<std::decay_t<Callable>> make_guard(Callable&& callable)
    {
        return scope_guard_generic<std::decay_t<Callable>>{ std::forward<Callable>(callable) };
    }

    // Todo: Exception guard
}