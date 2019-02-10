#pragma once
#include <optional>
#include <tuple>
#include <variant>

namespace meta
{
    template <typename T>
    struct type_base
    {
        using type = T;
    };

    namespace detail
    {
#ifdef META_LEGACY
        namespace v1
        {
            template <typename T, typename U, typename ...Types>
            struct is_within : bool_or<typename std::is_same<T, U>::type, 
                                       typename is_within<T, Types...>::type> {};

            template <typename T, typename U>
            struct is_within<T, U> : std::is_same<T, U>::type {};
        }
#endif  // META_LEGACY
        namespace v2
        {
            template <typename T, typename ...Types>
            struct is_within_impl : std::disjunction<std::is_same<T, Types>...> {};
        }

        using v2::is_within_impl;

        template <typename T>
        struct value_trait;

        template <typename T>
        struct value_trait<std::atomic<T>> : type_base<T> {};

        template <typename T, typename U, size_t I>
        struct is_same_indexed : std::is_same<T, U>
        {
            using left_argument = T;
            using right_argument = U;
            constexpr static size_t index = I;
        };

        template <typename...>
        struct index_impl;

        template <typename T, size_t ...Indexes, typename ...Types>
        struct index_impl<T, std::index_sequence<Indexes...>, Types...>
        {
            using type = std::disjunction<is_same_indexed<T, Types, Indexes>...>;
        };
    }

    template <typename T, typename ...Types>
    struct is_within : detail::is_within_impl<T, Types...>
    {
        static_assert(sizeof...(Types) > 1);
    };

    template <typename T>
    struct is_within<T> : std::false_type {};

    template <typename T, typename U>
    struct is_within<T, U> : std::is_same<T, U> {};

    template <typename T, typename ...Types>
    struct is_within<T, std::variant<Types...>> : is_within<T, Types...> {};

    template <typename T, typename ...Types>
    struct is_within<T, std::tuple<Types...>> : is_within<T, Types...> {};

    template <typename T, typename U, typename V>
    struct is_within<T, std::pair<U, V>> : is_within<T, U, V> {};

    template <typename T>
    struct add_const_ref : std::add_lvalue_reference_t<std::add_const_t<std::decay_t<T>>> {};

    template <typename T>
    using add_const_ref_t = typename add_const_ref<T>::type;

    template <typename T> //reference operation precedes
    struct remove_cv_ref : std::remove_cv<std::remove_reference_t<T>> {};

    template <typename T>
    using remove_cv_ref_t = typename remove_cv_ref<T>::type;

    template <typename T, typename ...Types>
    struct is_similar : std::conjunction<std::is_same<remove_cv_ref_t<T>,
                                                      remove_cv_ref_t<Types>>...>
    {
        static_assert(sizeof...(Types) > 0);
    };

    template <typename T>
    using value = typename detail::value_trait<remove_cv_ref_t<T>>::type;

    template <typename T>
    struct is_atomic : std::is_same<remove_cv_ref_t<T>,
                                    std::atomic<value<T>>> {};

    template <template<bool> typename B>
    struct is_bool_constant : std::conjunction<std::is_same<B<true>, std::true_type>,
                                               std::is_same<B<false>, std::false_type>> {};

    template <typename T, typename ...Types>
    struct max_size : std::integral_constant<size_t, std::max<size_t>(max_size<T>::value,
                                                                      max_size<Types...>::value)> {};

    template <typename T>
    struct max_size<T> : std::integral_constant<size_t, sizeof(T)> {};

    template <typename ...Types>
    struct max_size<std::variant<Types...>> : max_size<Types...> {};

    template <typename ...Types>
    struct max_size<std::tuple<Types...>> : max_size<Types...> {};

    template <typename T, typename U>
    struct max_size<std::pair<T, U>> : max_size<T, U> {};

    template <typename T, typename ...Types>
    struct index : std::integral_constant<
            size_t, detail::index_impl<T, std::index_sequence_for<Types...>,
                                       Types...>::type::index>
    {
        static_assert(meta::is_within<T, Types...>::value, "T is outside Types... pack");
    };

    template <typename T, typename ...Types>
    struct index<T, std::variant<Types...>>
        : std::integral_constant<size_t, index<T, Types...>::value> {};

    template <typename T, typename ...Types>
    struct index<T, std::tuple<Types...>>
        : std::integral_constant<size_t, index<T, Types...>::value> {};

    template <typename T, typename = void>
    struct is_hashable : std::false_type {};

    template <typename T>
    struct is_hashable<T, std::void_t<decltype(
                           std::hash<std::decay_t<T>>{}(std::declval<std::decay_t<T>&>()))>
        > : std::true_type {};

    template <typename Handle, typename = void>
    struct has_operator_dereference : std::false_type {};

    template <typename Handle>
    struct has_operator_dereference<
            Handle, std::void_t<decltype(
                std::declval<const std::decay_t<Handle>&>().operator->())>
        > : std::true_type {};

    template <typename V>
    struct is_variant : std::false_type {};

    template <typename ...Types>
    struct is_variant<std::variant<Types...>> : std::true_type {};

    template <typename T, typename Condition>
    struct has_if : std::false_type
    {
        using has_type = typename std::conditional<
            std::is_base_of<std::true_type, Condition>::value,
            typename has_if<T, std::true_type>::has_type,
            struct void_type
        >::type;
    };

    template <typename T>
    struct has_if<T, std::true_type> : std::true_type
    {
        using has_type = typename remove_cv_ref<T>::type;
    };
}
