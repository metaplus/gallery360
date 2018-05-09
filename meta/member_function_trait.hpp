#pragma once

namespace meta
{
    template<typename Return, typename Object, bool HasConst, typename... Args>
    struct member_function_trait
    {
        template<typename R, typename U, typename... As>
        explicit constexpr member_function_trait(R(U::*)(As...)const) { }

        template<typename R, typename U, typename... As>
        explicit constexpr member_function_trait(R(U::*)(As...)) { }

        using return_type = Return;
        using object_type = Object;
        using args_tuple = std::tuple<Args...>;

        static constexpr bool has_args = sizeof...(Args);
        static constexpr bool has_const = HasConst;
    };

    template<typename R, typename U, typename... As>
    member_function_trait(R(U::*)(As...)const) -> member_function_trait<R, U, true, As...>;

    template<typename R, typename U, typename... As>
    member_function_trait(R(U::*)(As...)) -> member_function_trait<R, U, false, As...>;

    template<auto MemFuncPtr>
    struct member_function
    {
        using type = decltype(MemFuncPtr);
        using trait = decltype(member_function_trait{ MemFuncPtr });
        using return_type = trait::template return_type;
        using object_type = trait::template object_type;
        using args_tuple = trait::template args_tuple;
        template<size_t Index>
        using nth_arg = std::tuple_element_t<Index, args_tuple>;

        static constexpr bool has_args = trait::template has_args;
        static constexpr bool has_const = trait::template has_const;
    };
}