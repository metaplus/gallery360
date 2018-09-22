#pragma once

namespace meta
{
    template<typename T, typename ...Types>
    struct is_within : detail::is_within_impl<T, Types...>
    {
        static_assert(sizeof...(Types) > 1);
    };

    template<typename T>
    struct is_within<T> : std::false_type {};

    template<typename T, typename U>
    struct is_within<T, U> : std::is_same<T, U> {};

    template<typename T, typename ...Types>
    struct is_within<T, std::variant<Types...>> : is_within<T, Types...> {};

    template<typename T, typename ...Types>
    struct is_within<T, std::tuple<Types...>> : is_within<T, Types...> {};

    template<typename T, typename U, typename V>
    struct is_within<T, std::pair<U, V>> : is_within<T, U, V> {};

    template<typename T>
    struct add_const_ref : std::add_lvalue_reference_t<std::add_const_t<std::decay_t<T>>> {};

    template<typename T>
    using add_const_ref_t = typename add_const_ref<T>::type;

    template<typename T> //reference operation precedes
    struct remove_cv_ref : std::remove_cv<std::remove_reference_t<T>> {};

    template<typename T>
    using remove_cv_ref_t = typename remove_cv_ref<T>::type;

    template<typename T, typename ...Types>
    struct is_similar : std::conjunction<std::is_same<remove_cv_ref_t<T>, remove_cv_ref_t<Types>>...>
    {
        static_assert(sizeof...(Types) > 0);
    };

    template<typename T>
    using value = typename detail::value_trait<remove_cv_ref_t<T>>::type;

#if __has_include(<!folly/futures/Future.h>)
    template<typename T>
    struct is_future : is_within<remove_cv_ref_t<T>, std::future<value<T>>, std::shared_future<value<T>>,
        folly::Future<value<T>>, folly::SemiFuture<value<T>>
    #ifdef CORE_USE_BOOST_FIBER
        , boost::fibers::future<value<T>>
    #endif //CORE_USE_BOOST_FIBER
    >
    {};
#endif

#if __has_include(<!folly/futures/Promise.h>)
    template<typename T>
    struct is_promise : is_within<remove_cv_ref_t<T>, std::promise<value<T>>, folly::Promise<value<T>>
    #ifdef CORE_USE_BOOST_FIBER
        , boost::fibers::promise<value<T>>
    #endif //CORE_USE_BOOST_FIBER
    >
    {};
#endif

    template<typename T>
    struct is_atomic : std::is_same<remove_cv_ref_t<T>, std::atomic<value<T>>> {};

    template<template<bool> typename B>
    struct is_bool_constant : std::conjunction<std::is_same<B<true>, std::true_type>, std::is_same<B<false>, std::false_type>> {};

    template<typename T>
    struct is_packaged_task : std::false_type {};

    template<typename Result, typename ...Args>
    struct is_packaged_task<std::packaged_task<Result(Args ...)>> : std::true_type {};

    template<typename Result, typename ...Args>
    struct is_packaged_task<boost::packaged_task<Result(Args ...)>> : std::true_type {};

    template<typename T, typename ...Types>
    struct max_size : std::integral_constant<size_t, std::max<size_t>(max_size<T>::value, max_size<Types...>::value)> {};

    template<typename T>
    struct max_size<T> : std::integral_constant<size_t, sizeof(T)> {};

    template<typename ...Types>
    struct max_size<std::variant<Types...>> : max_size<Types...> {};

    template<typename ...Types>
    struct max_size<std::tuple<Types...>> : max_size<Types...> {};

    template<typename T, typename U>
    struct max_size<std::pair<T, U>> : max_size<T, U> {};

    template<typename T, typename ...Types>
    struct index : std::integral_constant<size_t, detail::index_impl<T, std::index_sequence_for<Types...>, Types...>::type::index>
    {
        static_assert(meta::is_within<T, Types...>::value, "T is outside Types... pack");
    };

    template<typename T, typename ...Types>
    struct index<T, std::variant<Types...>> : std::integral_constant<size_t, index<T, Types...>::value> {};

    template<typename T, typename ...Types>
    struct index<T, std::tuple<Types...>> : std::integral_constant<size_t, index<T, Types...>::value> {};

    template<typename T, typename = void>
    struct is_hashable : std::false_type {};

    template<typename T>
    struct is_hashable < T, std::void_t<decltype(std::hash<std::decay_t<T>>{}(std::declval<std::decay_t<T>&>())) >
    > : std::true_type
    {};

    template<typename Handle, typename = void>
    struct has_operator_dereference : std::false_type {};

    template<typename Handle>
    struct has_operator_dereference<Handle, std::void_t<decltype(std::declval<const std::decay_t<Handle>&>().operator->())>
    > : std::true_type
    {};

    template<typename Exception>
    struct is_exception : std::disjunction<
        std::is_base_of<std::exception, Exception>,
        std::is_base_of<boost::exception, Exception>> {};

    template<typename V>
    struct is_variant : std::false_type {};

    template<typename ...Types>
    struct is_variant<std::variant<Types...>> : std::true_type {};
}
