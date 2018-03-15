#pragma once
#undef min  //abolish vicious macros from <windows.h>, otherwise causing naming collision against STL
#undef max  //another tolerable solution appears like #define max_RESUME max #undef max ... #define max max_RESUME
namespace core
{
    template<typename T>
    constexpr std::string_view type_shortname()
    {
        auto type_name = std::string_view{ typeid(T).name() };
        auto begin_iter = std::find(type_name.crbegin(), type_name.crend(), ':');
        if (begin_iter == type_name.crend())
            begin_iter = std::find(type_name.crbegin(), type_name.crend(), ' ');
        type_name.remove_prefix(std::distance(begin_iter, type_name.crend()));
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
        return meta::impl::make_array(
            meta::impl::sequence_arithmetic<std::plus<void>, Source>(
                meta::impl::sequence_arithmetic<std::multiplies<void>, Stride>(
                    std::make_integer_sequence<int, element_count>{})));
    }

    namespace literals
    {
        constexpr size_t operator""_kbyte(const size_t n) { return n * 1024; }
        constexpr size_t operator""_mbyte(const size_t n) { return n * 1024 * 1024; }
        constexpr size_t operator""_gbyte(const size_t n) { return n * 1024 * 1024 * 1024; }
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

    size_t directory_entry_count(const std::experimental::filesystem::path& directory);

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
}