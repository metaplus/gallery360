#pragma once

namespace meta
{
    template<typename Return, typename... Args>
    struct function_trait
    {
        template<typename R, typename... As>
        explicit constexpr function_trait(R(*)(As...)) { }

        template<typename R, typename... As>
        explicit constexpr function_trait(R(&)(As...)) { }

        using return_type = Return;
        using args_tuple = std::tuple<Args...>;

        static constexpr bool has_args = sizeof...(Args) > 0;
    };

    template<typename R, typename... As>
    function_trait(R(*)(As...)) -> function_trait<R, As...>;

    template<typename R, typename... As>
    function_trait(R(&)(As...)) -> function_trait<R, As...>;

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