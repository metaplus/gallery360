#pragma once

namespace core::meta
{
    template <typename Return, typename Object, bool HasConst, typename... Args>
    struct member_function_trait final
    {
        explicit constexpr member_function_trait(Return (Object::*)(Args ...) const) {}

        explicit constexpr member_function_trait(Return (Object::*)(Args ...)) {}

        using return_type = Return;
        using object_type = Object;
        using args_tuple = std::tuple<Args...>;

        static constexpr bool has_args = sizeof...(Args) > 0;
        static constexpr bool has_const = HasConst;
    };

    template <typename Return, typename Object, typename... Args>
    member_function_trait(Return (Object::*)(Args ...) const) -> member_function_trait<Return, Object, true, Args...>;

    template <typename Return, typename Object, typename... Args>
    member_function_trait(Return (Object::*)(Args ...)) -> member_function_trait<Return, Object, false, Args...>;

    template <auto MemFuncPtr>
    struct member_function final
    {
        using type = decltype(MemFuncPtr);
        using trait = decltype(member_function_trait{ MemFuncPtr });
#ifdef __linux__
        using return_type = typename trait::return_type;
        using object_type = typename trait::object_type;
        using args_tuple = typename trait::args_tuple;
#else
        using return_type = typename trait::template return_type;
        using object_type = typename trait::template object_type;
        using args_tuple = typename trait::template args_tuple;
#endif
        template <size_t Index>
        using nth_arg = std::tuple_element_t<Index, args_tuple>;

        static constexpr bool has_args = trait::template has_args;
        static constexpr bool has_const = trait::template has_const;
    };
}
