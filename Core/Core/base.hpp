#pragma once
#undef min  //abolish vicious macros from <windows.h>, otherwise causing naming collision against STL
#undef max  //another tolerable solution appears like #define max_RESUME max #undef max ... #define max max_RESUME
namespace core
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
    struct is_within<T, std::variant<Types...>> : core::is_within<T, Types...> {};
    template<typename T, typename ...Types>
    struct is_within<T, std::tuple<Types...>> : core::is_within<T, Types...> {};
    template<typename T, typename U, typename V>
    struct is_within<T, std::pair<U, V>> : core::is_within<T, U, V> {};
    template<typename T, typename... Types>
    constexpr bool is_within_v = is_within<T, Types...>::value;
    template<typename T>    //reference operation precedes
    struct remove_cv_ref : std::remove_cv<std::remove_reference_t<T>> {};
    template<typename T>
    using remove_cv_ref_t = typename remove_cv_ref<T>::type;
    template<typename T, typename ...Types>
    struct is_similar : std::conjunction<std::is_same<core::remove_cv_ref_t<T>, core::remove_cv_ref_t<Types>>...> { static_assert(sizeof...(Types) > 0); };
    template<typename T>
    using value_trait = impl::value_trait<core::remove_cv_ref_t<T>>;
    template<typename T>
    using value = typename core::value_trait<T>::type;
    template<typename T>
    struct is_future : core::is_within<core::remove_cv_ref_t<T>,
        std::future<core::value<T>>, std::shared_future<core::value<T>>> {};
    template<typename T>
    constexpr bool is_future_v = core::is_future<T>::value;
    template<typename T>
    struct is_atomic : std::is_same<core::remove_cv_ref_t<T>, std::atomic<core::value<T>>> {};
    template<typename T>
    constexpr bool is_atomic_v = core::is_atomic<T>::value;
    template<template<bool> typename B>
    struct is_bool_constant : std::conjunction<std::is_same<B<true>, std::true_type>, std::is_same<B<false>, std::false_type>> {};
    using relative = std::int64_t;
    using absolute = std::uint64_t;
    using rel = relative;
    using abs = absolute;
    template<typename T, typename ...Types>
    struct max_size : std::integral_constant<size_t, std::max<size_t>(core::max_size<T>::value, core::max_size<Types...>::value)> {};
    template<typename T>
    struct max_size<T> : std::integral_constant<size_t, sizeof(T)> {};
    template<typename ...Types>
    struct max_size<std::variant<Types...>> : core::max_size<Types...> {};
    template<typename ...Types>
    struct max_size<std::tuple<Types...>> : core::max_size<Types...> {};
    template<typename T, typename U>
    struct max_size<std::pair<T, U>> : core::max_size<T, U> {};
    /**
    *  @return compile-time generated [Source,Dest] array, divided by Stride
    *  @note will be refactored to generic after template<auto> supported, then complement core::sequence
    */
    template<int Source, int Dest, int Stride = 1>
    constexpr auto range()
    {
        static_assert(std::numeric_limits<int>::min() <= std::min<int>(Source, Dest));
        static_assert(std::numeric_limits<int>::max() >= std::max<int>(Source, Dest));
        static_assert(Source != Dest && Stride != 0 && ((Source < Dest) ^ (Stride < 0)));
        constexpr auto element_count = std::divides<int>{}(Dest - Source + Stride, Stride);
        return impl::make_array(impl::sequence_arithmetic<std::plus<void>, Source>(
            impl::sequence_arithmetic<std::multiplies<void>, Stride>(std::make_integer_sequence<int, element_count>{})));
    }
    namespace literals
    {
        constexpr size_t operator""_kbyte(const size_t n) { return n * 1024; }
        constexpr size_t operator""_mbyte(const size_t n) { return n * 1024 * 1024; }
        constexpr size_t operator""_gbyte(const size_t n) { return n * 1024 * 1024 * 1024; }
    }
    template<typename Enum, typename Offset = std::make_signed_t<std::underlying_type_t<Enum>>>
    constexpr auto enum_next(Enum e, Offset offset) -> std::enable_if_t<std::is_enum_v<Enum>, Enum>
    {
        return static_cast<Enum>(std::plus<void>{}(static_cast<std::underlying_type_t<Enum>>(e), offset));
    }
    template<typename Enum, typename Offset = std::make_signed_t<std::underlying_type_t<Enum>>>
    constexpr auto enum_advance(Enum& e, Offset offset) -> std::enable_if_t<std::is_enum_v<Enum>>
    {
        e = core::enum_next(e, offset);
    }
    inline auto directory_entry_count(const std::experimental::filesystem::path& directory)
    {   //non-recursive version, regardless of symbolic link
        const std::experimental::filesystem::directory_iterator iterator{ directory };
        return std::distance(begin(iterator), end(iterator));
    }
    inline auto thread_id = [deform = std::hash<std::thread::id>{}](std::optional<std::thread::id> id = std::nullopt)
    {
        return deform(id.value_or(std::this_thread::get_id()));
    };
    template<typename Callable, typename ...Types>
    Callable&& repeat(size_t count, Callable&& callable, Types&& ...args)
    {
        std::invoke(callable, args...);
        return --count == 0 ? std::forward<Callable>(callable) :
            core::repeat<Callable, Types...>(count, std::forward<Callable>(callable), std::forward<Types>(args)...);
    }
    template<typename Callable, typename ...Types>
    Callable&& repeat_each(Callable&& callable, Types&& ...args)
    {
        (..., std::invoke(callable, std::forward<Types>(args)));
        return std::forward<Callable>(callable);
    }
    template<typename Expr>
    constexpr bool identify(Expr&& condition)
    {
        using expr_type = core::remove_cv_ref_t<Expr>;
        if constexpr(std::is_same_v<expr_type, bool>)
            return condition;
        else if constexpr(std::is_null_pointer_v<expr_type>)
            return false;
        else if constexpr(std::is_pointer_v<expr_type> && !std::is_member_pointer_v<expr_type>)
            return condition != nullptr;
        else if constexpr(core::is_atomic<expr_type>::value && std::is_lvalue_reference_v<Expr>)
            return condition.load(std::memory_order_acquire);
        else if constexpr(std::is_invocable_r_v<bool, expr_type>)
            return std::invoke(std::forward<Expr>(condition));
        else
            static_assert(false, "taste undesirable expression");
    }
    template<typename Expr, typename Callable, typename... Types>
    Callable&& condition_loop(Expr&& condition, Callable&& callable, Types&& ...args)
    {
        while (core::identify(condition))
            std::invoke(callable, args...);
        return std::forward<Callable>(callable);
    }
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
        struct indexer;
        template<typename T, size_t ...Indexes, typename ...Types>
        struct indexer <T, std::index_sequence<Indexes...>, Types...>
        {
            using type = std::disjunction<is_same_indexed<T, Types, Indexes>...>;
        };
    }
    template<typename T, typename ...Types>
    struct indexer : std::integral_constant<size_t, impl::indexer<T, std::index_sequence_for<Types...>, Types...>::type::index>
    {
        static_assert(core::is_within_v<T, Types...>, "T is outside Types... pack");
    };
    template<typename T, typename ...Types>
    struct indexer <T, std::variant<Types...>> : std::integral_constant<size_t, indexer<T, Types...>::value> {};
    template<typename T, typename ...Types>
    struct indexer <T, std::tuple<Types...>> : std::integral_constant<size_t, indexer<T, Types...>::value> {};
    template<typename T>
    constexpr std::string_view type_shortname()
    {
        auto type_name = std::string_view{ typeid(T).name() };
        auto begin_pos = std::find(type_name.crbegin(), type_name.crend(), ':');
        if (begin_pos == type_name.crend())
            begin_pos = std::find(type_name.crbegin(), type_name.crend(), ' ');
        type_name.remove_prefix(std::distance(begin_pos, type_name.crend()));
        return type_name;
    }
}