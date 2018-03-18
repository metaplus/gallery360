#pragma once

namespace meta::detail
{
    namespace v1
    {
        template<typename T, typename U, typename ...Types>
        struct is_within : bool_or<typename std::is_same<T, U>::type, typename is_within<T, Types...>::type> {};
        template<typename T, typename U>
        struct is_within<T, U> : std::is_same<T, U>::type {};
    }

    inline namespace v2
    {
        template<typename T, typename ...Types>
        struct is_within : std::disjunction<std::is_same<T, Types>...> {};
    }

    template<typename T>
    struct value_trait;

    template<typename T>
    struct value_trait<std::atomic<T>> { using type = T; };

    template<typename T>
    struct value_trait<std::future<T>> { using type = T; };

    template<typename T>
    struct value_trait<std::shared_future<T>> { using type = T; };

    template<typename T, typename U, size_t I>
    struct is_same_indexed : std::is_same<T, U>
    {
        using left_type = T;
        using right_type = U;
        constexpr static size_t index = I;
    };

    template<typename...>
    struct index;

    template<typename T, size_t ...Indexes, typename ...Types>
    struct index <T, std::index_sequence<Indexes...>, Types...>
    {
        using type = std::disjunction<is_same_indexed<T, Types, Indexes>...>;
    };
}