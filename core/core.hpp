#pragma once

namespace core
{
    template<typename T>
    std::string type_shortname(const T* = nullptr)
    {
        std::string type_name{ typeid(T).name() };
        type_name.erase(0, type_name.find_last_of(": ") + 1);
        return type_name;
    }

    template<typename T>
    std::string type_shortname(const T&)
    {
        return type_shortname<std::remove_cv_t<T>>();
    }

    template<typename T>
    std::string type_name(const T* = nullptr)
    {
        std::string type_name{ typeid(T).name() };
        type_name.erase(0, type_name.rfind(' ') + 1);
        return type_name;
    }

    template<typename T>
    std::string type_name(const T&)
    {
        return type_name<std::remove_cv_t<T>>();
    }

    inline std::string time_string(std::string_view tformat = "%c"sv, std::tm*(*tfunc)(const std::time_t*) = &std::localtime)
    {
        std::ostringstream ostream;
        // const auto time_tmt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        const auto time_tmt = std::time(nullptr);
        const auto time_tm = *tfunc(&time_tmt);
        ostream << std::put_time(&time_tm, tformat.data());
        return ostream.str();
    }

    template<auto Begin, auto End, auto Span = 1>
    constexpr auto range_sequence()
    {
        static_assert(std::is_same_v<decltype(Begin), decltype(End)>);
        static_assert(Begin != End && Span != 0 && ((Begin < End) ^ (Span < 0)));
        constexpr auto count = std::divides<void>{}(End - Begin + Span, Span);
        return meta::sequence_plus<Begin>(meta::sequence_multiply<Span>(std::make_integer_sequence<decltype(count), count>{}));
    }

    template<auto Begin, auto End, auto Span = 1>
    constexpr auto range()
    {
        return meta::make_array(range_sequence<Begin, End, Span>());
    }

    namespace literals
    {
        constexpr size_t operator""_kbyte(const size_t n) { return n * 1024; }
        constexpr size_t operator""_mbyte(const size_t n) { return n * 1024 * 1024; }
        constexpr size_t operator""_gbyte(const size_t n) { return n * 1024 * 1024 * 1024; }

        template<typename Represent, typename Period>
        std::ostream& operator<<(std::ostream& os, const std::chrono::duration<Represent, Period>& dura)
        {
            using namespace std::chrono;
            return
                dura < 1us ? os << duration_cast<duration<double, std::nano>>(dura).count() << "ns" :
                dura < 1ms ? os << duration_cast<duration<double, std::micro>>(dura).count() << "us" :
                dura < 1s ? os << duration_cast<duration<double, std::milli>>(dura).count() << "ms" :
                dura < 1min ? os << duration_cast<duration<double>>(dura).count() << "s" :
                dura < 1h ? os << duration_cast<duration<double, std::ratio<60>>>(dura).count() << "min" :
                os << duration_cast<duration<double, std::ratio<3600>>>(dura).count() << "h";
        }
    }

    template<typename Enum>
    constexpr Enum enum_next(Enum e, std::make_signed_t<std::underlying_type_t<Enum>> offset)
    {
        return static_cast<Enum>(std::plus<void>{}(static_cast<std::underlying_type_t<Enum>>(e), offset));
    }

    template<typename Enum>
    constexpr Enum enum_prev(Enum e, std::make_signed_t<std::underlying_type_t<Enum>> offset)
    {
        return static_cast<Enum>(std::minus<void>{}(static_cast<std::underlying_type_t<Enum>>(e), offset));
    }

    template<typename Enum>
    constexpr Enum& enum_advance(Enum& e, std::make_signed_t<std::underlying_type_t<Enum>> offset)
    {
        return static_cast<std::underlying_type_t<Enum&>>(e) += offset;
    }

    inline size_t count_entry(const std::filesystem::path& directory)
    {
        //  non-recursive version, regardless of symbolic link
        const std::filesystem::directory_iterator iterator{ directory };
        return std::distance(begin(iterator), end(iterator));
    }

    inline size_t thread_hash_id(const std::thread::id id)
    {
        return std::hash<std::thread::id>{}(id);
    }

    inline size_t thread_hash_id()
    {
        return thread_hash_id(std::this_thread::get_id());
    }

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

    template <typename T>
    std::reference_wrapper<T> make_null_reference_wrapper() noexcept
    {
        static void* null_pointer = nullptr;
        return std::reference_wrapper<T>{ *reinterpret_cast<std::add_pointer_t<std::decay_t<T>>&>(null_pointer) };
    }

    // enable DefaultConstructible
    template<typename T>
    class reference : public std::reference_wrapper<T>
    {
    public:
        using std::reference_wrapper<T>::reference_wrapper;
        using std::reference_wrapper<T>::operator=;
        using std::reference_wrapper<T>::operator();
        reference() noexcept
            : std::reference_wrapper<T>(core::make_null_reference_wrapper<T>())
        {}
    };

    inline namespace tag    //  tag dispatching usage, clarify semantics
    {
        struct use_future_t {};
        inline constexpr use_future_t use_future{};

        struct use_recursion_t{};
        inline constexpr use_recursion_t use_recursion;

        struct as_default_t {};
        inline constexpr as_default_t as_default;

        struct as_element_t {};
        inline constexpr as_element_t as_element;

        struct as_observer_t {};
        inline constexpr as_observer_t as_observer;

        struct defer_construct_t {};
        inline constexpr defer_construct_t defer_construct;

        struct defer_execute_t {};
        inline constexpr defer_execute_t defer_execute;

        struct defer_destruct_t {};
        inline constexpr defer_destruct_t defer_destruct;
    }

    namespace v1
    {
        template<typename T, typename ...Types>
        size_t hash_value(T&& a, Types&& ...args)
        {
            static_assert((meta::is_hashable<T>::value && ... && meta::is_hashable<Types>::value));
            std::ostringstream ss{ std::ios::out | std::ios::binary };
            ss << std::hex;
            ((ss << std::setw(sizeof(size_t) * 2) << std::setfill('\0')
                << std::hash<meta::remove_cv_ref_t<T>>{}(std::forward<T>(a)))
                << ...
                << std::hash<meta::remove_cv_ref_t<Types>>{}(std::forward<Types>(args)));
            return std::hash<std::string>{}(ss.str());
        }
    }

    namespace v2
    {
        template<typename ...Types>
        size_t hash_value(Types&& ...args) noexcept
        {
            static_assert(sizeof...(Types) > 0, "require at least 1 argument");
            static_assert((... && meta::is_hashable<Types>::value), "require hashable");
            std::array<size_t, sizeof...(Types)> carray{ std::hash<std::decay_t<Types>>{}(args)... };
            return std::hash<std::string_view>{}({ reinterpret_cast<char*>(carray.data()), sizeof(carray) });
        }
        
        template<typename ...Types>
        struct hash
        {
            size_t operator()(const std::decay_t<Types>& ...args) const noexcept
            {
                return hash_value(args...);
            }
        };
    }

    namespace v3
    {
        namespace detail
        {
            template<typename T, typename ...Types>
            auto hash_value_tuple(const T& head, const Types& ...tails) noexcept;

            template<typename T>
            std::tuple<size_t> hash_value_tuple(const T& head) noexcept
            {
                return std::make_tuple(std::hash<T>{}(head));
            }

            template<typename T, typename U>
            std::tuple<size_t, size_t> hash_value_tuple(const std::pair<T, U>& head) noexcept
            {
                return hash_value_tuple(head.first, head.second);
            }

            template<typename ...TupleTypes>
            auto hash_value_tuple(const std::tuple<TupleTypes...>& head) noexcept
            {
                return hash_value_tuple(std::get<TupleTypes>(head)...);
            }

            template<typename T, typename ...Types>
            auto hash_value_tuple(const T& head, const Types& ...tails) noexcept
            {
                return std::tuple_cat(hash_value_tuple(head), hash_value_tuple(tails...));
            }
        }

        template<typename ...Types>
        size_t hash_value_from(const Types& ...args) noexcept
        {
            static_assert(sizeof...(Types) > 0);
            const auto tuple = detail::hash_value_tuple(args...);
            return std::hash<std::string_view>{}(std::string_view{ reinterpret_cast<const char*>(&tuple), sizeof tuple });
        }

        template<typename ...Types>
        struct hash
        {
            size_t operator()(const Types& ...args) noexcept
            {
                return hash_value_from(args);
            }
        };

        template<>
        struct hash<void>
        {
            template<typename ...Types>
            size_t operator()(const Types& ...args) const noexcept
            {
                return hash_value_from(args...);
            }
        };
    }

    using v3::hash_value_from;
    using v3::hash;

    template<typename Hash>
    struct dereference_hash
    {   // smart pointer or iterator
        template<typename Handle>
        size_t operator()(const Handle& handle) const { return Hash{}(*handle); }
    };

    template<typename Handle>
    decltype(auto) get_pointer(Handle&& handle, std::enable_if_t<meta::has_operator_dereference<Handle>::value>* = nullptr)
    {
        return std::forward<Handle>(handle).operator->();
    }

    template<typename Handle>
    decltype(auto) get_pointer(Handle&& handle, std::enable_if_t<std::is_pointer_v<std::decay_t<Handle>>>* = nullptr) noexcept
    {
        return std::forward<Handle>(handle);
    }

    template<typename T>
    [[nodiscard]] constexpr std::remove_const_t<T>& as_mutable(T& lval) noexcept
    {
        return const_cast<std::remove_const_t<T>&>(lval);
    }

    template<typename T>
    void as_mutable(const T&&) = delete;

    template<typename Mandator>
    struct dereference_delegate
    {
        template<typename ...Handles>
        decltype(auto) operator()(Handles&& ...args) const
            //    ->  std::invoke_result_t<std::decay_t<Callable>, decltype(*std::forward<Handles>(args))...>
        {
            return std::decay_t<Mandator>{}((*std::forward<Handles>(args))...);
        }
    };

    template<typename T, typename U>
    constexpr bool address_same(const T& x, const U& y) noexcept
    {
        return std::addressof(x) == std::addressof(y);
    }

    struct noncopyable
    {
    protected:
        constexpr noncopyable() noexcept = default;
        ~noncopyable() = default;
        noncopyable(const noncopyable&) = delete;
        noncopyable& operator=(const noncopyable&) = delete;
    };

    template<typename T, typename Equal = std::equal_to<T>>
    constexpr bool is_default_constructed(const T& object) noexcept
    {
        static_assert(std::is_default_constructible_v<T>);
        return Equal{}(object, T{});
    }
}