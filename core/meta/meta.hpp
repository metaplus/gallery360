#pragma once

namespace meta
{
    template<typename Callable>
    struct invoke
    {
        template<typename ...Types>
        constexpr static std::decay_t<std::invoke_result_t<Callable, Types...>> by(Types ...args)
        {
            static_assert(std::conjunction<std::is_trivially_copyable<Types>...>::value);
            //static_assert(std::is_standard_layout_v<std::common_type_t<Types...>>);
            return Callable{}(args...);
        }

        template<auto ...Args>
        struct apply
        {
            static constexpr auto value = Callable{}(Args...);
        };
    };

    template<typename BinaryCallable, auto Factor, auto... Values>
    constexpr auto sequence_arithmetic(
        std::integer_sequence<std::common_type_t<decltype(Values)...>, Values...>)
    {
        using factor_type = decltype(Factor);
        using value_type = std::common_type_t<decltype(Values)...>;
        static_assert(std::is_invocable_r_v<value_type, BinaryCallable, value_type, factor_type>);
        return std::integer_sequence<value_type, invoke<BinaryCallable>::by(Values, Factor)...>{};
    }

    template<auto Factor, auto... Values>
    constexpr auto sequence_plus(
        std::integer_sequence<std::common_type_t<decltype(Values)...>, Values...> sequence)
    {
        return sequence_arithmetic<std::plus<void>>(sequence);
    }

    template<auto Factor, auto... Values>
    constexpr auto sequence_minus(
        std::integer_sequence<std::common_type_t<decltype(Values)...>, Values...> sequence)
    {
        return sequence_arithmetic<std::minus<void>>(sequence);
    }

    template<auto Factor, auto... Values>
    constexpr auto sequence_multiply(
        std::integer_sequence<std::common_type_t<decltype(Values)...>, Values...> sequence)
    {
        return sequence_arithmetic<std::multiplies<void>>(sequence);
    }

    template<auto Factor, auto... Values>
    constexpr auto sequence_divide(
        std::integer_sequence<std::common_type_t<decltype(Values)...>, Values...> sequence)
    {
        return sequence_arithmetic<std::divides<void>>(sequence);
    }

    template<typename T, typename ...Types>
    struct reverse_tuple
    {
        using type = decltype(std::tuple_cat(std::declval<typename reverse_tuple<Types...>::type&>(),
                                             std::declval<std::tuple<T>&>()));
        constexpr static size_t size = 1 + sizeof...(Types);
    };

    template<typename T>
    struct reverse_tuple<T>
    {
        using type = std::tuple<T>;
        constexpr static size_t size = 1;
    };

    template<typename Indexes, typename ...Types>
    struct indexed_tuple;

    template<size_t ...Indexes, typename ...Types>
    struct indexed_tuple<std::index_sequence<Indexes...>, Types...>
    {
        using type = std::tuple<std::tuple_element_t<Indexes, std::tuple<Types...>>...>;
        constexpr static size_t size = sizeof...(Types);
    };

    template<typename ...Types, size_t ...Indexes>
    std::tuple<std::tuple_element_t<Indexes, std::tuple<Types...>>...>
    make_indexed_tuple(std::tuple<Types...> const& tuple,
                       std::index_sequence<Indexes...>  = std::index_sequence_for<Types...>{})
    {
        return std::make_tuple(std::get<Indexes>(tuple)...);
    }

    template<typename T, T ...Values>
    constexpr std::array<T, sizeof...(Values)>
    make_array(std::integer_sequence<T, Values...>  = {})
    {
        return { Values... };
    }

    template<typename ...Types>
    constexpr std::array<std::common_type_t<Types...>, sizeof...(Types)>
    make_array_any(Types const& ...args)
    {
        return { static_cast<std::common_type_t<Types...>>(args)... };
    }
}
