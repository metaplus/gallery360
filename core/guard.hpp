#pragma once

namespace core
{
    class time_guard final
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

    template <typename Callable>
    class scope_guard final : protected std::decay_t<Callable>
    {
    public:
        explicit scope_guard(const std::decay_t<Callable>&) = delete;

        explicit scope_guard(std::decay_t<Callable>&& callable)
            : std::decay_t<Callable>(std::move(callable)) {}

        scope_guard() = delete;
        scope_guard(const scope_guard&) = delete;
        scope_guard(scope_guard&&) noexcept = default;
        scope_guard& operator=(const scope_guard&) = delete;
        scope_guard& operator=(scope_guard&&) noexcept = default;

        ~scope_guard() {
            operator()();
        }

    protected:
        using std::decay<Callable>::type::operator();
    };

    template <typename Callable>
    scope_guard<std::decay_t<Callable>> make_guard(Callable&& callable) {
        return scope_guard<std::decay_t<Callable>>{ std::forward<Callable>(callable) };
    }

    template <typename Callable>
    scope_guard(std::decay_t<Callable>&& callable) -> scope_guard<std::decay_t<Callable>>;

    namespace v1 // TODO: experimental
    {
        template <typename... Callable>
        class scope_guard_tuple final : protected std::decay_t<Callable>...
        {
        public:
            explicit scope_guard_tuple(std::decay_t<Callable>&&... callable)
                : std::decay_t<Callable>(std::move(callable))... {}

            ~scope_guard_tuple() {
                (..., std::decay_t<Callable>::operator()());
            }

        protected:
            using std::decay<Callable>::type::operator()...;
        };

        template <typename... Callable>
        scope_guard_tuple<std::decay_t<Callable>...> make_guard(Callable&&... callable) {
            return scope_guard_tuple<std::decay_t<Callable>...>{ std::forward<Callable>(callable)... };
        }
    }
}
