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

    namespace v2
    {
        template<typename Callable>
        class generic_guard : Callable
        {
        public:
            explicit generic_guard(Callable&& c) : Callable(std::forward<Callable>(c)) {}
            generic_guard() = default;
            generic_guard(const generic_guard&) = delete;
            generic_guard(generic_guard&&) = default;
            generic_guard& operator=(const generic_guard&) = delete;
            generic_guard& operator=(generic_guard&& other) = default;
            using Callable::operator();
            ~generic_guard() { (*this)(); }
        };

        template<typename Callable>
        generic_guard<std::decay_t<Callable>> make_guard(Callable&& callable)
        {
            return generic_guard<std::decay_t<Callable>>{ std::forward<Callable>(callable) };
        }
    }
    // Todo: Exception guard
}