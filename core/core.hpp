#pragma once

namespace core
{
    template<typename BufferSequence, typename ...TailSequence>
    std::list<boost::asio::const_buffer>
        split_buffer_sequence(BufferSequence&& sequence, TailSequence&& ...tails) {
        static_assert(boost::asio::is_const_buffer_sequence<decltype(sequence.data())>::value);
        std::list<boost::asio::const_buffer> buffer_list;
        auto sequence_data = std::forward<BufferSequence>(sequence).data();
        std::transform(boost::asio::buffer_sequence_begin(sequence_data),
                       boost::asio::buffer_sequence_end(sequence_data),
                       std::back_inserter(buffer_list),
                       [](const auto& buffer) { return boost::asio::const_buffer{ buffer }; });
        if constexpr (sizeof...(TailSequence) > 0) {
            buffer_list.splice(buffer_list.end(),
                               split_buffer_sequence(std::forward<TailSequence>(tails)...));
        }
        return buffer_list;
    }

    std::string time_format(std::string format = "%c"s,
                            std::tm*(*timing)(std::time_t const*) = &std::localtime);

    template<auto Begin, auto End, auto Span = 1>
    constexpr auto range_sequence() {
        static_assert(std::is_same<decltype(Begin), decltype(End)>::value);
        static_assert(Begin != End && Span != 0 && ((Begin < End) ^ (Span < 0)));
        constexpr auto count = std::divides<void>{}(End - Begin + Span, Span);
        return meta::sequence_plus<Begin>(meta::sequence_multiply<Span>(
            std::make_integer_sequence<std::common_type_t<decltype(Begin), decltype(End)>, count>{}));
    }

    template<auto Begin, auto End, auto Span = 1>
    constexpr auto range() {
        return meta::make_array(range_sequence<Begin, End, Span>());
    }

    namespace literals
    {
        constexpr size_t operator""_kbyte(size_t const n) {
            return n * 1024;
        }

        constexpr size_t operator""_mbyte(size_t const n) {
            return n * 1024 * 1024;
        }

        constexpr size_t operator""_gbyte(size_t const n) {
            return n * 1024 * 1024 * 1024;
        }

        template<typename Represent, typename Period>
        std::ostream& operator<<(std::ostream& os, std::chrono::duration<Represent, Period> const& dura) {
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

    size_t count_file_entry(std::filesystem::path const& directory);

    template<typename T>
    std::reference_wrapper<T> make_null_reference_wrapper() noexcept {
        static void* null_pointer = nullptr;
        return std::reference_wrapper<T>{
            *reinterpret_cast<std::add_pointer_t<std::decay_t<T>>&>(null_pointer)
        };
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
            : std::reference_wrapper<T>(core::make_null_reference_wrapper<T>()) {}
    };

    inline namespace tag //  tag dispatching usage, clarify semantics
    {
        inline constexpr struct use_future_tag {} use_future;
        inline constexpr struct use_recursion_tag {} use_recursion;
        inline constexpr struct as_default_tag {} as_default;
        inline constexpr struct as_stacktrace_tag {} as_stacktrace;
        inline constexpr struct as_element_tag {} as_element;
        inline constexpr struct as_view_tag {} as_view;
        inline constexpr struct as_observer_tag {} as_observer;
        inline constexpr struct defer_construct_tag {} defer_construct;
        inline constexpr struct defer_execute_tag {} defer_execute;
        inline constexpr struct defer_destruct_tag {} defer_destruct;
        inline constexpr struct folly_tag {} folly;
        inline constexpr struct boost_tag {} boost;
        inline constexpr struct tbb_tag {} tbb;
    }

    namespace v3
    {
        namespace detail
        {
            template<typename T, typename ...Types>
            auto hash_value_tuple(T const& head, Types const& ...tails) noexcept;

            template<typename T>
            std::tuple<size_t> hash_value_tuple(T const& head) noexcept {
                return std::make_tuple(std::hash<T>{}(head));
            }

            template<typename T, typename U>
            std::tuple<size_t, size_t> hash_value_tuple(std::pair<T, U> const& head) noexcept {
                return hash_value_tuple(head.first, head.second);
            }

            template<typename ...TupleTypes>
            auto hash_value_tuple(std::tuple<TupleTypes...> const& head) noexcept {
                return hash_value_tuple(std::get<TupleTypes>(head)...);
            }

            template<typename T, typename ...Types>
            auto hash_value_tuple(T const& head, Types const& ...tails) noexcept {
                return std::tuple_cat(hash_value_tuple(head), hash_value_tuple(tails...));
            }
        }

        template<typename ...Types>
        size_t hash_value_from(Types const& ...args) noexcept {
            static_assert(sizeof...(Types) > 0);
            const auto tuple = detail::hash_value_tuple(args...);
            return std::hash<std::string_view>{}(std::string_view{ reinterpret_cast<const char*>(&tuple), sizeof tuple });
        }

        template<typename ...Types>
        struct byte_hash
        {
            size_t operator()(Types const& ...args) noexcept {
                return hash_value_from(args);
            }
        };

        template<>
        struct byte_hash<void>
        {
            template<typename ...Types>
            size_t operator()(Types const& ...args) const noexcept {
                return hash_value_from(args...);
            }
        };
    }

    using v3::byte_hash;
    using v3::hash_value_from;

    template<typename Hash>
    struct deref_hash
    {
        // smart pointer or iterator
        template<typename Handle>
        size_t operator()(Handle const& handle) const {
            return Hash{}(*handle);
        }
    };

    template<typename Handle>
    decltype(auto) get_pointer(Handle&& handle, std::enable_if_t<meta::has_operator_dereference<Handle>::value>* = nullptr) {
        return std::forward<Handle>(handle).operator->();
    }

    template<typename Pointee>
    Pointee* const& get_pointer(Pointee* const& handle) noexcept {
        return handle;
    }

    template<typename T>
    [[nodiscard]] constexpr std::remove_const_t<T>& as_mutable(T& object) noexcept {
        return const_cast<std::remove_const_t<T>&>(object);
    }

    template<typename T>
    [[nodiscard]] constexpr T& as_mutable(const T* ptr) noexcept {
        assert(ptr != nullptr);
        return const_cast<T&>(*ptr);
    }

    template<typename T>
    void as_mutable(T const&&) = delete;

    template<typename T, typename U>
    constexpr bool address_same(T const& x, U const& y) noexcept {
        return std::addressof(x) == std::addressof(y);
    }

    template<typename Enum>
    constexpr std::underlying_type_t<Enum> underlying(Enum const& enumeration) noexcept {
        static_assert(std::is_enum<Enum>::value);
        return static_cast<std::underlying_type_t<Enum>>(enumeration);
    }

    template<typename EnumT, typename EnumU>
    constexpr bool underlying_same(EnumT const& et, EnumU const& eu) noexcept {
        return std::equal_to<>{}(underlying(et), underlying(eu));
    }

    template<typename ...Types>
    struct overload : Types... { using Types::operator()...; };
    template<typename ...Types> overload(Types...)->overload<Types...>;

    template<typename Variant, typename ...Callable>
    auto visit(Variant&& variant, Callable&& ...callable) {
        static_assert(meta::is_variant<std::decay_t<Variant>>::value);
        return std::visit(
            overload{ std::forward<Callable>(callable)... },
            std::forward<Variant>(variant));
    }

    void set_cpu_executor(int concurrency, int queue_size, std::string_view pool_name = "CorePool");

    void set_cpu_executor(int concurrency, std::string_view pool_name = "CorePool");

    template<typename T, typename ...Policy>
    static auto promise_contract_of(Policy& ...p) {
        if constexpr (meta::is_within<folly_tag, Policy...>::value) {
            return folly::makePromiseContract<T>();
        } else if constexpr (meta::is_within<folly::Promise<T>, Policy...>::value) {
            auto tuple = std::forward_as_tuple(p...);
            folly::Promise<T>& promise = std::get<folly::Promise<T>&>(tuple);
            auto future = promise.getSemiFuture();
            return std::make_pair(std::move(promise), std::move(future));
        } else if constexpr (meta::is_within<boost::promise<T>, Policy...>::value) {
            auto tuple = std::forward_as_tuple(p...);
            boost::promise<T>& promise = std::get<boost::promise<T>&>(tuple);
            auto future = promise.get_future();
            return std::make_pair(std::move(promise), std::move(future));
        } else {
            boost::promise<T> promise;
            auto future = promise.get_future();
            return std::make_pair(std::move(promise), std::move(future));
        }
    }
}
