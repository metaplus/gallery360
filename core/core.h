#pragma once

namespace core
{
    template<typename T>
    constexpr std::string_view type_shortname()
    {
        auto type_name = std::string_view{ typeid(T).name() };
        auto iter_begin = std::find(type_name.crbegin(), type_name.crend(), ':');
        if (iter_begin == type_name.crend())
            iter_begin = std::find(type_name.crbegin(), type_name.crend(), ' ');
        type_name.remove_prefix(std::distance(iter_begin, type_name.crend()));
        return type_name;
    }

    std::string time_string(std::string_view tformat = "%c"sv, std::tm*(*tfunc)(const std::time_t*) = &std::localtime);

    template<int Source, int Dest, int Stride = 1>
    constexpr auto range()
    {
        static_assert(std::numeric_limits<int>::min() <= std::min<int>(Source, Dest));
        static_assert(std::numeric_limits<int>::max() >= std::max<int>(Source, Dest));
        static_assert(Source != Dest && Stride != 0 && ((Source < Dest) ^ (Stride < 0)));
        constexpr auto element_count = std::divides<int>{}(Dest - Source + Stride, Stride);
        return meta::make_array(
            meta::sequence_arithmetic<std::plus<void>, Source>(
                meta::sequence_arithmetic<std::multiplies<void>, Stride>(
                    std::make_integer_sequence<int, element_count>{})));
    }

    namespace literals
    {
        constexpr size_t operator""_kbyte(const size_t n) { return n * 1024; }
        constexpr size_t operator""_mbyte(const size_t n) { return n * 1024 * 1024; }
        constexpr size_t operator""_gbyte(const size_t n) { return n * 1024 * 1024 * 1024; }

        template<typename Represent, typename Period>
        std::ostream& operator<<(std::ostream& os, std::chrono::duration<Represent, Period> dura)
        {
            using namespace std::chrono;
            return
                dura < 1ms ? os << duration_cast<duration<double, std::micro>>(dura).count() << "us" :
                dura < 1s ? os << duration_cast<duration<double, std::milli>>(dura).count() << "ms" :
                dura < 1min ? os << duration_cast<duration<double>>(dura).count() << "s" :
                dura < 1h ? os << duration_cast<duration<double, std::ratio<60>>>(dura).count() << "min" :
                os << duration_cast<duration<double, std::ratio<3600>>>(dura).count() << "h";
        }
    }

    template<typename Enum, typename Offset = std::make_signed_t<std::underlying_type_t<Enum>>>
    constexpr std::enable_if_t<std::is_enum_v<Enum>, Enum> enum_next(Enum e, Offset offset)
    {
        return static_cast<Enum>(std::plus<void>{}(static_cast<std::underlying_type_t<Enum>>(e), offset));
    }

    template<typename Enum, typename Offset = std::make_signed_t<std::underlying_type_t<Enum>>>
    constexpr std::enable_if_t<std::is_enum_v<Enum>> enum_advance(Enum& e, Offset offset)
    {
        e = core::enum_next(e, offset);
    }

    size_t count_entry(const std::experimental::filesystem::path& directory);

    size_t thread_hash_id(std::optional<std::thread::id> id = std::nullopt);

    template<typename Callable, typename ...Types>
    Callable&& repeat(size_t count, Callable&& callable, Types&& ...args)
    {
        std::invoke(callable, args...);
        return --count == 0 ? std::forward<Callable>(callable) :
            core::repeat<Callable, Types...>(count, std::forward<Callable>(callable), std::forward<Types>(args)...);
    }

    template<typename Callable, typename ...Types>
    Callable&& repeat_each(Callable&& callable, Types&& ...args)
    {
        (..., std::invoke(callable, std::forward<Types>(args)));
        return std::forward<Callable>(callable);
    }

    template<typename Expr>
    constexpr bool identify(Expr&& condition)
    {
        using expr_type = meta::remove_cv_ref_t<Expr>;
        if constexpr(std::is_same_v<expr_type, bool>)
            return condition;
        else if constexpr(std::is_null_pointer_v<expr_type>)
            return false;
        else if constexpr(std::is_pointer_v<expr_type> && !std::is_member_pointer_v<expr_type>)
            return condition != nullptr;
        else if constexpr(meta::is_atomic<expr_type>::value && std::is_lvalue_reference_v<Expr>)
            return condition.load(std::memory_order_acquire);
        else if constexpr(std::is_invocable_r_v<bool, expr_type>)
            return std::invoke(std::forward<Expr>(condition));
        else
            static_assert(false, "taste undesirable expression");
    }

    template<typename Expr, typename Callable, typename... Types>
    Callable&& condition_loop(Expr&& condition, Callable&& callable, Types&& ...args)
    {
        while (core::identify(condition))
            std::invoke(callable, args...);
        return std::forward<Callable>(callable);
    }

    template <typename T>
    std::reference_wrapper<T> make_empty_reference_wrapper()
    {
        static void* lval_nullptr = nullptr;
        return std::reference_wrapper<T>{ *reinterpret_cast<std::add_pointer_t<T>&>(lval_nullptr) };
    }

    using relative = std::int64_t;
    using absolute = std::uint64_t;
    using rel = relative;
    using abs = absolute;

    template<typename Future>
    std::enable_if_t<meta::is_future<Future>::value>
        persist_wait(Future&& future, std::atomic<bool>& permit, std::chrono::steady_clock::duration interval = 0ns)
    {
        auto attempt = future.wait_for(0ns);
        if (attempt == std::future_status::deferred)
            throw std::invalid_argument{ "prohibit deferred future, otherwise inevitably suffers infinite blocking potential" };
        while (attempt != std::future_status::ready)
        {
            if (!permit.load(std::memory_order_acquire))
                throw force_exit_exception{};
            attempt = future.wait_for(interval);
        }
    }

    template<typename Callable>
    std::enable_if_t<std::is_invocable_v<Callable>>
        persist_wait(Callable callable, std::atomic<bool>& permit, std::chrono::steady_clock::duration interval = 0ns)
    {
        auto future = std::async(std::move(callable));
        while (future.wait_for(interval) != std::future_status::ready)
            if (!permit.load(std::memory_order_acquire))
                throw force_exit_exception{};
    }

    struct use_future_t {};                                 // tag dispatch for future overload
    inline constexpr use_future_t use_future{};
}