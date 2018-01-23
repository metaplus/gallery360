#pragma once
#undef min	//abolish harmful c-style macro from <windows.h>
#undef max

namespace core
{
    template<typename T, typename U>
    struct bool_and;
    template<bool T, bool U>
    struct bool_and<std::bool_constant<T>, std::bool_constant<U>> : std::bool_constant<T&&U> {};
    template<typename T, typename U>
    struct bool_or;
    template<bool T, bool U>
    struct bool_or<std::bool_constant<T>, std::bool_constant<U>> : std::bool_constant<T || U> {};
    namespace impl
    {
        //TODO test & refine
        template<typename Func, typename T, typename ...Types >
        std::invoke_result_t<Func(T, T)>
            binary_recursion(T&& first, T&& second, Types&& ...tails)
        {   //require tests
            return sizeof...(tails) > 0 ?
                binary_recursion<Func, T>(                         //std::forward<Types>(tails)...
                    Func(std::forward(first), std::forward(second)), std::forward(tails)...) :
                Func(first, second);
        }
        /*template<typename T,typename U,typename... Types>
        struct within {
            constexpr static bool value = std::is_same_v<T, U> || within<T, Types...>::y;
        };
        template<typename T,typename U>
        struct within<T, U> {
            constexpr static bool value = std::is_same_v<T, U>;
        };*/
        template<typename T, typename U, typename... Types>
        struct within :
            bool_or<
            typename std::is_same<T, U>::type,
            typename within<T, Types...>::type> {};
        template<typename T, typename U>
        struct within<T, U> :
            std::is_same<T, U>::type {};
        template<typename T, T ...Vals>
        auto make_array(std::integer_sequence<T, Vals...> = {})
        {
            return std::array<T, sizeof...(Vals)>{ Vals... };
        }
        template<typename T, T Base, T ...Vals>
        auto offset(std::integer_sequence<T, Vals...> = {})
        {
            return std::integer_sequence<T, (Base + Vals)...>{};
        }
    }
    //TypeTrait: is_within
    template<typename T, typename... Types>
    struct is_within :impl::within<T, Types...> {
        static_assert(sizeof...(Types) > 1, "...Types has at least 2 elements");
    };
    template<typename T, typename... Types>
    constexpr bool is_within_v = is_within<T, Types...>::value;
    template<typename T>
    struct is_future :std::false_type {};
    template<typename T>
    struct is_future<std::future<T>> :std::true_type {};
    template<typename T>
    struct is_future<std::shared_future<T>> :std::true_type {};
    using relative = std::int64_t;
    using absolute = std::uint64_t;
    using rel = relative;
    using abs = absolute;
    /**
    *  @return compile-time generated [Left,Right] array.
    *  @note will be refactored after template<auto> supported.
    */
    template<int Left, int Right>
    auto range()
    {
        static_assert(std::numeric_limits<int>::min() <= Left);
        static_assert(std::numeric_limits<int>::max() >= Right);
        return impl::make_array(impl::offset<int, Left>(
            std::make_integer_sequence<int, Right - Left + 1>{}));
    }
    namespace literals
    {
        constexpr size_t operator""_kilo(const size_t n) { return n * 1024; }
        constexpr size_t operator""_mega(const size_t n) { return n * 1024 * 1024; }
        constexpr size_t operator""_giga(const size_t n) { return n * 1024 * 1024 * 1024; }
    }
    inline auto count_entry(const std::experimental::filesystem::path& directory)
    {
        const std::experimental::filesystem::directory_iterator iterator{ directory };
        return std::distance(begin(iterator), end(iterator));
    }
    inline auto thread_id = [hash = std::hash<std::thread::id>{}]
    (std::optional<std::thread::id> id = std::nullopt)
    {
        return hash(id.value_or(std::this_thread::get_id()));
    };
    inline decltype(auto) repeat = [](auto count, auto&& callable, auto&& ...args)
    {
        static_assert(std::is_integral_v<decltype(count)>);
        if (callable(std::forward<decltype(args)>(args)...); --count > 0)
            return repeat(count, std::forward(callable), std::forward<decltype(args)>(args)...);
        else
            return std::forward(callable);
    };
    inline decltype(auto) repeat_each = [](auto&& callable, auto&& ...args)
    {
        return std::forward(callable(std::forward(args))...);
    };
}

//static_assert(std::is_same_v<byte, uint8_t>);   //not std::byte
