#pragma once

namespace meta
{
    template<typename Return, typename... Args>
    struct function_trait
    {
        template<typename Return, typename... Args>
        explicit constexpr function_trait(Return(*)(Args...)) { }

        template<typename Return, typename... As>
        explicit constexpr function_trait(Return(&)(Args...)) { }

        using return_type = Return;
        using args_tuple = std::tuple<Args...>;

        static constexpr bool has_args = sizeof...(Args) > 0;
    };

    template<typename Return, typename... Args>
    function_trait(Return(*)(Args...)) -> function_trait<Return, Args...>;

    template<typename Return, typename... Args>
    function_trait(Return(&)(Args...)) -> function_trait<Return, Args...>;

    template<auto FreeFuncPtr>
    struct function
    {
        using type = decltype(FreeFuncPtr);
        using trait = decltype(function_trait{ FreeFuncPtr });
        using return_type = trait::template return_type;
        using args_tuple = trait::template args_tuple;
        template<size_t Index>
        using nth_arg = std::tuple_element_t<Index, args_tuple>;

        static constexpr bool has_args = trait::template has_args;
    };
}