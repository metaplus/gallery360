#pragma once

namespace meta
{
    template <typename Return, typename... Args>
    struct function_trait final
    {
        explicit constexpr function_trait(Return (*)(Args ...)) {}

        explicit constexpr function_trait(Return (&)(Args ...)) {}

        using return_type = Return;
        using args_tuple = std::tuple<Args...>;

        static constexpr bool has_args = sizeof...(Args) > 0;
    };

    template <typename Return, typename... Args>
    function_trait(Return (*)(Args ...)) -> function_trait<Return, Args...>;

    template <typename Return, typename... Args>
    function_trait(Return (&)(Args ...)) -> function_trait<Return, Args...>;

    template <auto FreeFuncPtr>
    struct function final
    {
        using type = decltype(FreeFuncPtr);
        using trait = decltype(function_trait{ FreeFuncPtr });
#ifdef __linux__
        using return_type = typename trait::return_type;
        using args_tuple = typename trait::args_tuple;
#else
        using return_type = typename trait::template return_type;
        using args_tuple = typename trait::template args_tuple;
#endif
        template <size_t Index>
        using nth_arg = typename std::tuple_element<Index, args_tuple>::type;

        static constexpr bool has_args = trait::template has_args;
    };
}
