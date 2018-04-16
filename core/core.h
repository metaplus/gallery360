#pragma once

namespace core
{
    template<typename T>
    std::string type_shortname(std::add_pointer_t<T> = nullptr)
    {
        std::string type_name{ typeid(T).name() };
        type_name.erase(0, type_name.find_last_of(": ") + 1);
        return type_name;
    }

    template<typename T>
    std::string type_name(std::add_pointer_t<T> = nullptr)
    {
        std::string type_name{ typeid(T).name() };
        type_name.erase(0, type_name.rfind(' ') + 1);
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
                dura < 1us ? os << duration_cast<duration<double, std::nano>>(dura).count() << "ns" :
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
    std::reference_wrapper<T> make_null_reference_wrapper()
    {
        static void* lval_nullptr = nullptr;
        return std::reference_wrapper<T>{
            *reinterpret_cast<std::add_pointer_t<std::decay_t<T>>&>(lval_nullptr) };
    }

    // enable DefaultConstructible
    template<typename T>
    class reference : public std::reference_wrapper<T>
    {
    public:
        using std::reference_wrapper<T>::reference_wrapper;
        reference()
            : std::reference_wrapper<T>(core::make_null_reference_wrapper<T>())
        {
        }
    };

    using relative = std::int64_t;
    using absolute = std::uint64_t;
    using rel = relative;
    using abs = absolute;

    template<typename Future, typename Represent, typename Period>
    std::enable_if_t<meta::is_future<Future>::value> persist_wait(Future&& future,
        std::atomic<bool>& permit, const std::chrono::duration<Represent, Period> interval = 0ns)
    {
        auto attempt = future.wait_for(0ns);
        if (attempt == std::future_status::deferred)
            throw std::invalid_argument{ "prohibit deferred future, otherwise inevitably suffers infinite blocking potential" };
        while (attempt != std::future_status::ready)
        {
            if (!permit.load(std::memory_order_acquire))
                throw aborted_error{};
            attempt = future.wait_for(interval);
        }
    }

    template<typename Callable, typename Represent, typename Period>
    std::enable_if_t<std::is_invocable_v<Callable>> persist_wait(Callable callable,
        std::atomic<bool>& permit, const std::chrono::duration<Represent, Period> interval = 0ns)
    {
        auto future = std::async(std::move(callable));
        while (future.wait_for(interval) != std::future_status::ready)
            if (!permit.load(std::memory_order_acquire))
                throw aborted_error{};
    }

    inline namespace tag    // tag dispatching usage, clarify semantics
    {
        struct use_future_t {};
        inline constexpr use_future_t use_future{};

        struct as_default_t {};
        inline constexpr as_default_t as_default;

        struct as_element_t {};
        inline constexpr as_element_t as_element;

        struct defer_construct_t {};
        inline constexpr defer_construct_t defer_construct;

        struct defer_execute_t {};
        inline constexpr defer_execute_t defer_execute;

        struct defer_destruct_t {};
        inline constexpr defer_destruct_t defer_destruct;
    }

    namespace v1
    {
        template<typename T, typename ...Types>
        size_t hash_value(T&& a, Types&& ...args)
        {
            static_assert((meta::is_hashable<T>::value && ... && meta::is_hashable<Types>::value));
            std::ostringstream ss{ std::ios::out | std::ios::binary };
            ss << std::hex;
            ((ss << std::setw(sizeof(size_t) * 2) << std::setfill('\0')
                << std::hash<meta::remove_cv_ref_t<T>>{}(std::forward<T>(a)))
                << ...
                << std::hash<meta::remove_cv_ref_t<Types>>{}(std::forward<Types>(args)));
            return std::hash<std::string>{}(ss.str());
        }
    }

    template<typename ...Types>
    size_t hash_value(Types&& ...args)
    {
        static_assert(sizeof...(Types) > 0, "require at least 1 argument");
        static_assert((... && meta::is_hashable<Types>::value), "require hashable");
        std::array<size_t, sizeof...(Types)> carray{ std::hash<std::decay_t<Types>>{}(args)... };
        return std::hash<std::string_view>{}({ reinterpret_cast<char*>(carray.data()), sizeof(carray) });
    }

    template<typename ...Types>
    struct hash
    {
        size_t operator()(const std::decay_t<Types>& ...args) const noexcept
        {
            return hash_value(args...);
        }
    };

    template<typename Hash>
    struct dereference_hash
    {   // smart pointer or iterator
        template<typename Handle>
        size_t operator()(const Handle& handle) const { return Hash{}(*handle); }
    };
}