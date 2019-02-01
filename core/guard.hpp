#pragma once

namespace core
{
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
