#pragma once

namespace meta
{
    template<typename Callable>
    struct static_invoke
    {
        template<typename ...Types>
        constexpr static auto by(Types ...args) -> std::decay_t<std::invoke_result_t<Callable, Types...>>
        {
            static_assert((std::is_trivially_copyable_v<Types> && ...));
            //static_assert(std::is_standard_layout_v<std::common_type_t<Types...>>);
            return Callable{}(args...);
        }
    };

    template<typename BinaryCallable, int64_t Factor, typename T, T ...Values>
    constexpr auto sequence_arithmetic(std::integer_sequence<T, Values...> = {})
    {
        static_assert(std::is_invocable_r_v<T, BinaryCallable, const T&, const T&>);
        static_assert(std::is_convertible_v<decltype(Factor), T>);
        return std::integer_sequence<T, static_invoke<BinaryCallable>::by(Values, Factor)...>{};
    }

    template<typename T, typename ...Types>
    struct reverse_tuple
    {
        using type = decltype(std::tuple_cat(
            std::declval<typename reverse_tuple<Types...>::type>(), std::declval<std::tuple<T>>()));
        constexpr static size_t size = 1 + sizeof...(Types);
    };

    template<typename T>
    struct reverse_tuple<T>
    {
        using type = std::tuple<T>;
        constexpr static size_t size = 1;
    };

    template<typename BinaryCallable, typename, typename >
    struct bool_arithmetic;

    template<typename BinaryCallable, bool L, bool R>
    struct bool_arithmetic<BinaryCallable, std::bool_constant<L>, std::bool_constant<R>>
        : std::bool_constant<static_invoke<BinaryCallable>::by(L, R)>
    {};

    template<typename Bl, typename Br>    // for std::bool_constant AND operation
    struct bool_and : bool_arithmetic<std::logical_and<bool>, Bl, Br> {};

    template<typename Bl, typename Br>    // for std::bool_constant OR operation
    struct bool_or : bool_arithmetic<std::logical_or<bool>, Bl, Br> {};

    template<typename Bl, typename Br>    // for std::bool_constant XOR operation
    struct bool_xor : bool_arithmetic<std::bit_xor<bool>, Bl, Br> {};

    template<typename T, T ...Values>
    constexpr std::array<T, sizeof...(Values)> make_array(std::integer_sequence<T, Values...> = {})
    {
        return { Values... };
    }

    template<typename ...Types>
    constexpr std::array<std::common_type_t<Types...>, sizeof...(Types)> make_array_any(Types... args)
    {
        return { static_cast<std::common_type_t<Types...>>(args)... };
    }
}