#pragma once

namespace meta
{
    template<typename T, typename ...Types>
    struct is_within : detail::is_within<T, Types...> { static_assert(sizeof...(Types) > 1); };

    template<typename T, typename ...Types>
    struct is_within<T, std::variant<Types...>> : meta::is_within<T, Types...> {};

    template<typename T, typename ...Types>
    struct is_within<T, std::tuple<Types...>> : meta::is_within<T, Types...> {};

    template<typename T, typename U, typename V>
    struct is_within<T, std::pair<U, V>> : meta::is_within<T, U, V> {};

    template<typename T, typename... Types>
    constexpr bool is_within_v = is_within<T, Types...>::value;

    template<typename T>    //reference operation precedes
    struct remove_cv_ref : std::remove_cv<std::remove_reference_t<T>> {};

    template<typename T>
    using remove_cv_ref_t = typename remove_cv_ref<T>::type;

    template<typename T, typename ...Types>
    struct is_similar : std::conjunction<std::is_same<meta::remove_cv_ref_t<T>, meta::remove_cv_ref_t<Types>>...> { static_assert(sizeof...(Types) > 0); };

    template<typename T>
    using value_trait = detail::value_trait<meta::remove_cv_ref_t<T>>;

    template<typename T>
    using value = typename meta::value_trait<T>::type;

    template<typename T>
    struct is_future : meta::is_within<meta::remove_cv_ref_t<T>,
        std::future<meta::value<T>>, std::shared_future<meta::value<T>>> {};
    template<typename T>
    constexpr bool is_future_v = meta::is_future<T>::value;

    template<typename T>
    struct is_atomic : std::is_same<meta::remove_cv_ref_t<T>, std::atomic<meta::value<T>>> {};
    template<typename T>
    constexpr bool is_atomic_v = meta::is_atomic<T>::value;

    template<template<bool> typename B>
    struct is_bool_constant : std::conjunction<std::is_same<B<true>, std::true_type>, std::is_same<B<false>, std::false_type>> {};

    template<typename T>
    struct is_packaged_task : std::false_type {};

    template<typename Result, typename ...Args>
    struct is_packaged_task<std::packaged_task<Result(Args...)>> : std::true_type {};

    template<typename T>
    constexpr bool is_packaged_task_v = meta::is_packaged_task<T>::value;

    template<typename T, typename ...Types>
    struct max_size : std::integral_constant<size_t, std::max<size_t>(meta::max_size<T>::value, meta::max_size<Types...>::value)> {};

    template<typename T>
    struct max_size<T> : std::integral_constant<size_t, sizeof(T)> {};

    template<typename ...Types>
    struct max_size<std::variant<Types...>> : meta::max_size<Types...> {};

    template<typename ...Types>
    struct max_size<std::tuple<Types...>> : meta::max_size<Types...> {};

    template<typename T, typename U>
    struct max_size<std::pair<T, U>> : meta::max_size<T, U> {};

    template<typename T, typename ...Types>
    struct index : std::integral_constant<size_t, detail::index<T, std::index_sequence_for<Types...>, Types...>::type::index>
    {
        static_assert(meta::is_within_v<T, Types...>, "T is outside Types... pack");
    };

    template<typename T, typename ...Types>
    struct index <T, std::variant<Types...>> : std::integral_constant<size_t, index<T, Types...>::value> {};

    template<typename T, typename ...Types>
    struct index <T, std::tuple<Types...>> : std::integral_constant<size_t, index<T, Types...>::value> {};

    template<typename T, typename = void>
    struct is_hashable :std::false_type {};

    template<typename T>
    struct is_hashable<T, std::void_t<decltype(
        std::declval<std::hash<remove_cv_ref_t<T>>&>()(std::declval<const T&>()))>> :std::true_type {};
    
    template<typename T>
    constexpr bool is_hashable_v = is_hashable<T>::value;
}