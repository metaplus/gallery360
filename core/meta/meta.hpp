#pragma once

namespace meta
{
    template <typename Callable>
    struct invoke_expression final
    {
        template <typename ...Types>
        constexpr static std::decay_t<std::invoke_result_t<Callable, Types...>> by(Types ...args) {
            static_assert(std::conjunction<std::is_trivially_copyable<Types>...>::value);
            return Callable{}(args...);
        }

        template <auto ...Args>
        struct apply final
        {
            static constexpr auto value = Callable{}(Args...);
        };
    };

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
            struct is_within : bool_or<typename std::is_same<T, U>::type, typename is_within<T, Types...>::type> {};

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

        template <typename T>
        struct value_trait<std::promise<T>> : type_base<T> {};

#if __has_include(<!boost/thread/future.hpp>)
        template<typename T>
        struct value_trait<boost::promise<T>> : type_base<T> {};

        template<typename T>
        struct value_trait<boost::future<T>> : type_base<T> {};

        template<typename T>
        struct value_trait<boost::shared_future<T>> : type_base<T> {};
#endif

        template <typename T>
        struct value_trait<folly::Promise<T>> : type_base<T> {};

        template <typename T>
        struct value_trait<std::future<T>> : type_base<T> {};

        template <typename T>
        struct value_trait<std::shared_future<T>> : type_base<T> {};

        template <typename T>
        struct value_trait<folly::Future<T>> : type_base<T> {};

        template <typename T>
        struct value_trait<folly::SemiFuture<T>> : type_base<T> {};

#ifdef CORE_USE_BOOST_FIBER
        template<typename T>
        struct value_trait<boost::fibers::promise<T>> { using type = T; };

        template<typename T>
        struct value_trait<boost::fibers::future<T>> { using type = T; };
#endif  // CORE_USE_BOOST_FIBER

        template <typename T, typename U, size_t I>
        struct is_same_indexed : std::is_same<T, U>
        {
            using left_argument = T;
            using right_argument = U;
            constexpr static size_t index = I;
        };

        template <typename...>
        struct index_impl;

        template <typename T,
                  size_t ...Indexes,
                  typename ...Types>
        struct index_impl<T, std::index_sequence<Indexes...>, Types...>
        {
            using type = std::disjunction<is_same_indexed<T, Types, Indexes>...>;
        };
    }

    template <typename BinaryCallable, auto Factor, auto... Values>
    constexpr auto sequence_arithmetic(
        std::integer_sequence<std::common_type_t<decltype(Values)...>, Values...>) {
        using factor_type = decltype(Factor);
        using value_type = std::common_type_t<decltype(Values)...>;
        static_assert(std::is_invocable_r_v<value_type, BinaryCallable, value_type, factor_type>);
        return std::integer_sequence<value_type, invoke_expression<BinaryCallable>::by(Values, Factor)...>{};
    }

    template <auto Factor, auto... Values>
    constexpr auto sequence_plus(
        std::integer_sequence<std::common_type_t<decltype(Values)...>, Values...> sequence) {
        return sequence_arithmetic<std::plus<void>>(sequence);
    }

    template <auto Factor, auto... Values>
    constexpr auto sequence_minus(
        std::integer_sequence<std::common_type_t<decltype(Values)...>, Values...> sequence) {
        return sequence_arithmetic<std::minus<void>>(sequence);
    }

    template <auto Factor, auto... Values>
    constexpr auto sequence_multiply(
        std::integer_sequence<std::common_type_t<decltype(Values)...>, Values...> sequence) {
        return sequence_arithmetic<std::multiplies<void>>(sequence);
    }

    template <auto Factor, auto... Values>
    constexpr auto sequence_divide(
        std::integer_sequence<std::common_type_t<decltype(Values)...>, Values...> sequence) {
        return sequence_arithmetic<std::divides<void>>(sequence);
    }

    template <typename T, typename ...Types>
    struct reverse_tuple final
    {
        using type = decltype(std::tuple_cat(std::declval<typename reverse_tuple<Types...>::type&>(),
                                             std::declval<std::tuple<T>&>()));
        constexpr static size_t size = 1 + sizeof...(Types);
    };

    template <typename T>
    struct reverse_tuple<T> final
    {
        using type = std::tuple<T>;
        constexpr static size_t size = 1;
    };

    template <typename Indexes, typename ...Types>
    struct indexed_tuple;

    template <size_t ...Indexes, typename ...Types>
    struct indexed_tuple<std::index_sequence<Indexes...>, Types...> final
    {
        using type = std::tuple<std::tuple_element_t<Indexes, std::tuple<Types...>>...>;
        constexpr static size_t size = sizeof...(Types);
    };

    template <typename ...Types, size_t ...Indexes>
    std::tuple<std::tuple_element_t<Indexes, std::tuple<Types...>>...>
    make_indexed_tuple(const std::tuple<Types...>& tuple,
                       std::index_sequence<Indexes...>  = std::index_sequence_for<Types...>{}) {
        return std::make_tuple(std::get<Indexes>(tuple)...);
    }

    template <typename T, T ...Values>
    constexpr std::array<T, sizeof...(Values)>
    make_array(std::integer_sequence<T, Values...>  = {}) {
        return { Values... };
    }

    template <typename ...Types>
    constexpr std::array<std::common_type_t<Types...>, sizeof...(Types)>
    make_array_any(const Types& ...args) {
        return { static_cast<std::common_type_t<Types...>>(args)... };
    }

    template <typename T, bool Const = false, bool Copyable = false>
    struct access_functor final : type_base<folly::Function<T&()>> {};
    template <typename T>
    struct access_functor<T, true, false> final : type_base<folly::Function<T&() const>> {};
    template <typename T>
    struct access_functor<T, false, true> final : type_base<std::function<T&()>> {};
    template <typename T>
    struct access_functor<T, true, true> final : type_base<std::function<T&() const>> {};

    template <typename T, bool Const = false, bool Copyable = false>
    struct process_functor final : type_base<folly::Function<void(T&)>> {};
    template <typename T>
    struct process_functor<T, true, false> final : type_base<folly::Function<void(T&) const>> {};
    template <typename T>
    struct process_functor<T, false, true> final : type_base<std::function<void(T&)>> {};
    template <typename T>
    struct process_functor<T, true, true> final : type_base<std::function<void(T&) const>> {};
}
