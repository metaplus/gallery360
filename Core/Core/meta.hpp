#pragma once
namespace meta
{
    namespace impl
    {
        template<typename Callable>
        struct static_invoke
        {
            template<typename ...Types>
            constexpr static auto by(Types ...args) -> std::decay_t<std::invoke_result_t<Callable, Types...>>
            {
                static_assert((std::is_trivially_copyable_v<Types> && ...));
                //static_assert(std::is_standard_layout_v<std::common_type_t<Types...>>);
                return Callable{}(args...);
            }
        };
        template<typename BinaryCallable, typename, typename >
        struct bool_arithmetic;
        template<typename BinaryCallable, bool L, bool R>
        struct bool_arithmetic<BinaryCallable, std::bool_constant<L>, std::bool_constant<R>>
            : std::bool_constant<static_invoke<BinaryCallable>::by(L, R)> {};
        template<typename BinaryCallable, int64_t Factor, typename T, T ...Values>
        constexpr auto sequence_arithmetic(std::integer_sequence<T, Values...> = {})
        {
            static_assert(std::is_invocable_r_v<T, BinaryCallable, const T&, const T&>);
            static_assert(std::is_convertible_v<decltype(Factor), T>);
            return std::integer_sequence<T, static_invoke<BinaryCallable>::by(Values, Factor)...>{};
        }
    }
    template<typename Bl, typename Br>    //for std::bool_constant AND operation
    struct bool_and : impl::bool_arithmetic<std::logical_and<bool>, Bl, Br> {};
    template<typename Bl, typename Br>    //for std::bool_constant OR operation
    struct bool_or : impl::bool_arithmetic<std::logical_or<bool>, Bl, Br> {};
    template<typename Bl, typename Br>    //for std::bool_constant XOR operation
    struct bool_xor : impl::bool_arithmetic<std::bit_xor<bool>, Bl, Br> {};

    template<typename T, typename ...Types>
    struct reverse_tuple
    {
        using type = decltype(std::tuple_cat(
            std::declval<typename reverse_tuple<Types...>::type>(), std::declval<std::tuple<T>>()));
        constexpr static size_t size = 1 + sizeof...(Types);
    };
    template<typename T>
    struct reverse_tuple<T>
    {
        using type = std::tuple<T>;
        constexpr static size_t size = 1;
    };

    namespace impl
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

        template<typename T, T ...Values>
        constexpr std::array<T, sizeof...(Values)>
            make_array(std::integer_sequence<T, Values...> = {})
        {
            return { Values... };
        }
        template<typename ...Types>
        constexpr std::array<std::common_type_t<Types...>, sizeof...(Types)>
            make_array_any(Types... args)
        {
            return { static_cast<std::common_type_t<Types...>>(args)... };
        }

        template<typename T>
        struct value_trait;
        template<typename T>
        struct value_trait<std::atomic<T>> { using type = T; };
        template<typename T>
        struct value_trait<std::future<T>> { using type = T; };
        template<typename T>
        struct value_trait<std::shared_future<T>> { using type = T; };
    }
    template<typename T, typename ...Types>
    struct is_within : impl::is_within<T, Types...> { static_assert(sizeof...(Types) > 1); };
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
    using value_trait = impl::value_trait<meta::remove_cv_ref_t<T>>;
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

    using relative = std::int64_t;
    using absolute = std::uint64_t;
    using rel = relative;
    using abs = absolute;
    
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
    
    namespace impl
    {
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
    template<typename T, typename ...Types>
    struct index : std::integral_constant<size_t, impl::index<T, std::index_sequence_for<Types...>, Types...>::type::index>
    {
        static_assert(meta::is_within_v<T, Types...>, "T is outside Types... pack");
    };
    template<typename T, typename ...Types>
    struct index <T, std::variant<Types...>> : std::integral_constant<size_t, index<T, Types...>::value> {};
    template<typename T, typename ...Types>
    struct index <T, std::tuple<Types...>> : std::integral_constant<size_t, index<T, Types...>::value> {};
}