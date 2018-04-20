#pragma once

namespace core
{
    class time_guard
    {
    public:
        time_guard();
        time_guard(const time_guard&) = delete;
        time_guard(time_guard&&) noexcept = default;
        time_guard& operator=(const time_guard&) = delete;
        time_guard& operator=(time_guard&&) noexcept = default;
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
        std::function<void()> release_;         //  facilitate type erasure, efficiency depends on STL implementation
    };

    template<typename Callable>
    class scope_guard_generic : std::decay_t<Callable>
    {
    public:
        explicit scope_guard_generic(const std::decay_t<Callable>&) = delete;
        explicit scope_guard_generic(std::decay_t<Callable>&& callable);
        scope_guard_generic() = default;
        scope_guard_generic(const scope_guard_generic&) = delete;
        scope_guard_generic(scope_guard_generic&&) /*noexcept(noexcept(std::declval<std::decay_t<Callable>&>()()))*/ noexcept = default;
        scope_guard_generic& operator=(const scope_guard_generic&) = delete;
        scope_guard_generic& operator=(scope_guard_generic&&) /*noexcept(noexcept(std::declval<std::decay_t<Callable>&>()()))*/ noexcept = default;
        ~scope_guard_generic();
    private:
        using std::decay<Callable>::type::operator();
        //static_assert(!std::is_reference_v<Callable> && !std::is_const_v<Callable> && !std::is_volatile_v<Callable>);
        //static_assert(std::is_same_v<std::decay_t<Callable>, Callable>);
    };

    template <typename Callable>
    scope_guard_generic<Callable>::scope_guard_generic(std::decay_t<Callable>&& callable)
        : Callable(std::forward<Callable>(callable))
    {}

    template <typename Callable>
    scope_guard_generic<Callable>::~scope_guard_generic()
    {
        operator()();
    }

    template<typename Callable>
    scope_guard_generic<std::decay_t<Callable>> make_guard(Callable&& callable)
    {
        return scope_guard_generic<std::decay_t<Callable>>{ std::forward<Callable>(callable) };
    }

    //  Todo: make_multiple_guards(Callable&&... callables), consider multiple inheritance implementation

    //  Todo: exception guard
}