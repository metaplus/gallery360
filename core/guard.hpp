#pragma once

namespace core
{
    class time_guard
    {
    public:
        time_guard() :
            time_mark_(std::chrono::steady_clock::now()) {}
        time_guard(const time_guard&) = delete;
        time_guard(time_guard&&) noexcept = default;
        time_guard& operator=(const time_guard&) = delete;
        time_guard& operator=(time_guard&&) noexcept = default;
        ~time_guard() {
            //std::cout.setf(std::ios::hex);
            std::cout
                << "thread@" << std::this_thread::get_id() << ' '
                << std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(std::chrono::steady_clock::now() - time_mark_).count()
                << " ms\n";
            //std::cout.unsetf(std::ios::hex);
        }
    private:
        std::chrono::steady_clock::time_point time_mark_;
    };

    class scope_guard
    {
    public:
        explicit scope_guard(std::function<void()> release, bool ctor_invoke = false)
            : release_(ctor_invoke ? release : std::move(release)) {
            if (ctor_invoke)
                release();
        }
        scope_guard() = default;
        scope_guard(const scope_guard&) = delete;
        scope_guard(scope_guard&&) = default;
        scope_guard& operator=(const scope_guard&) = delete;
        scope_guard& operator=(scope_guard&& other) = default;
        ~scope_guard() {
            if (release_)
                release_();
        }

    private:
        std::function<void()> release_;         //  facilitate type erasure, efficiency depends on STL implementation
    };

    template<typename Callable>
    class scope_guard_generic : protected std::decay_t<Callable>
    {
    public:
        explicit scope_guard_generic(const std::decay_t<Callable>&) = delete;
        explicit scope_guard_generic(std::decay_t<Callable>&& callable)
            : std::decay_t<Callable>(std::move(callable)) {}
        scope_guard_generic() = delete;
        scope_guard_generic(const scope_guard_generic&) = delete;
        scope_guard_generic(scope_guard_generic&&) noexcept/*(std::is_nothrow_invocable_v<std::decay_t<Callable>>)*/ = default;
        scope_guard_generic& operator=(const scope_guard_generic&) = delete;
        scope_guard_generic& operator=(scope_guard_generic&&) noexcept/*(std::is_nothrow_invocable_v<std::decay_t<Callable>>)*/ = default;
        ~scope_guard_generic() {
            operator()();
        }

    protected:
        using std::decay<Callable>::type::operator();
    };

    namespace v2
    {
        template<typename Callable>
        scope_guard_generic<std::decay_t<Callable>> make_guard(Callable&& callable) {
            return scope_guard_generic<std::decay_t<Callable>>{ std::forward<Callable>(callable) };
        }
    }

    using v2::make_guard;

    template<typename Callable>
    scope_guard_generic(std::decay_t<Callable>&& callable)->scope_guard_generic<std::decay_t<Callable>>;

    namespace v1    // TODO: experimental
    {
        template<typename... Callable>
        class scope_guard_tuple : protected std::decay_t<Callable>...
        {
        public:
            explicit scope_guard_tuple(std::decay_t<Callable>&&... callable)
                : std::decay_t<Callable>(std::move(callable))...
            {}
            ~scope_guard_tuple() {
                (..., std::decay_t<Callable>::operator()());
                //(..., &scope_guard_generic<std::decay_t<Callable>>::operator()(this));
            }
        protected:
            using std::decay<Callable>::type::operator()...;
        };

        template<typename... Callable>
        scope_guard_tuple<std::decay_t<Callable>...> make_guard(Callable&&... callable) {
            return scope_guard_tuple<std::decay_t<Callable>...>{ std::forward<Callable>(callable)... };
        }
    }
}