#pragma once

namespace core
{
    template <typename BufferSequence, typename ...TailSequence>
    std::list<boost::asio::const_buffer>
    split_buffer_sequence(BufferSequence&& sequence, TailSequence&& ...tails) {
        static_assert(boost::asio::is_const_buffer_sequence<decltype(sequence.data())>::value);
        std::list<boost::asio::const_buffer> buffer_list;
        auto sequence_data = std::forward<BufferSequence>(sequence).data();
        std::transform(boost::asio::buffer_sequence_begin(sequence_data),
                       boost::asio::buffer_sequence_end(sequence_data),
                       std::back_inserter(buffer_list),
                       [](const auto& buffer) {
                           return boost::asio::const_buffer{ buffer };
                       });
        if constexpr (sizeof...(TailSequence) > 0) {
            buffer_list.splice(buffer_list.end(),
                               split_buffer_sequence(std::forward<TailSequence>(tails)...));
        }
        return buffer_list;
    }

    template <typename T, typename ...Args>
    std::shared_ptr<T>& access(std::shared_ptr<T>& ptr, Args&&... args) {
        if (!ptr) {
            ptr = std::make_shared<T>(std::forward<Args>(args)...);
        }
        return ptr;
    }

    template <typename Deleter, typename T, typename ...Args>
    std::shared_ptr<T>& access(std::shared_ptr<T>& ptr,
                               std::default_delete<T>&& deleter,
                               Args&&... args) {
        static_assert(std::is_base_of<std::default_delete<T>, Deleter>::value);
        if (!ptr) {
            ptr = std::shared_ptr<T>(
                new T{ std::forward<Args>(args)... },
                std::move(dynamic_cast<Deleter&&>(deleter)));
        }
        return ptr;
    }

    // Format template "%Y-%m-%d %H:%M:%S"
    std::string time_format(std::string_view format = "%c",
                            std::tm*(*timing)(std::time_t const*) = &std::localtime);

    std::string local_date_time();

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

        template <typename Represent, typename Period>
        std::ostream& operator<<(std::ostream& os, const std::chrono::duration<Represent, Period>& dura) {
            using namespace std::chrono;
            return
                dura < 1us
                    ? os << duration_cast<duration<double, std::nano>>(dura).count() << "ns"
                    : dura < 1ms
                          ? os << duration_cast<duration<double, std::micro>>(dura).count() << "us"
                          : dura < 1s
                                ? os << duration_cast<duration<double, std::milli>>(dura).count() << "ms"
                                : dura < 1min
                                      ? os << duration_cast<duration<double>>(dura).count() << "s"
                                      : dura < 1h
                                            ? os << duration_cast<duration<double, std::ratio<60>>>(dura).count() << "min"
                                            : os << duration_cast<duration<double, std::ratio<3600>>>(dura).count() << "h";
        }
    }

    size_t count_file_entry(const std::filesystem::path& directory);

    std::pair<size_t, bool> make_empty_directory(const std::filesystem::path& directory);

    template <typename EntryPredicate>
    std::vector<std::filesystem::path> filter_directory_entry(const std::filesystem::path& directory,
                                                              const EntryPredicate& predicate) {
        return std::reduce(
            std::filesystem::directory_iterator{ directory },
            std::filesystem::directory_iterator{},
            std::vector<std::filesystem::path>{},
            [&predicate](std::vector<std::filesystem::path>&& container, const std::filesystem::directory_entry& entry) {
                if (predicate(entry)) {
                    container.emplace_back(entry.path());
                }
                return container;
            });
    }

    std::filesystem::path tidy_directory_path(const std::filesystem::path& directory);

    std::filesystem::path file_path_of_directory(const std::filesystem::path& directory,
                                                 const std::filesystem::path& extension);

    std::filesystem::path last_write_path_of_directory(const std::filesystem::path& directory);

    struct last_write_time_comparator final
    {
        bool operator()(const std::filesystem::path& left, const std::filesystem::path& right) const;
        bool operator()(const std::filesystem::directory_entry& left, const std::filesystem::directory_entry& right) const;
    };

    template <typename T>
    std::reference_wrapper<T> make_null_reference_wrapper() noexcept {
        static void* null_pointer = nullptr;
        return std::reference_wrapper<T>{
            *reinterpret_cast<std::add_pointer_t<std::decay_t<T>>&>(null_pointer)
        };
    }

    // enable DefaultConstructable
    template <typename T>
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
        inline constexpr struct use_future_tag final {} use_future;

        inline constexpr struct as_stacktrace_tag final {} as_stacktrace;

        inline constexpr struct as_view_tag final {} as_view;

        inline constexpr struct defer_execute_tag final {} defer_execute;
    }

    namespace v3
    {
        namespace detail
        {
            template <typename T, typename ...Types>
            auto hash_value_tuple(const T& head, const Types& ...tails) noexcept;

            template <typename T>
            std::tuple<size_t> hash_value_tuple(const T& head) noexcept {
                return std::make_tuple(std::hash<T>{}(head));
            }

            template <typename T, typename U>
            std::tuple<size_t, size_t> hash_value_tuple(const std::pair<T, U>& head) noexcept {
                return hash_value_tuple(head.first, head.second);
            }

            template <typename ...TupleTypes>
            auto hash_value_tuple(const std::tuple<TupleTypes...>& head) noexcept {
                return hash_value_tuple(std::get<TupleTypes>(head)...);
            }

            template <typename T, typename ...Types>
            auto hash_value_tuple(const T& head,
                                  const Types& ...tails) noexcept {
                return std::tuple_cat(hash_value_tuple(head),
                                      hash_value_tuple(tails...));
            }
        }

        template <typename ...Types>
        size_t hash_value_from(Types const& ...args) noexcept {
            static_assert(sizeof...(Types) > 0);
            const auto tuple = detail::hash_value_tuple(args...);
            return std::hash<std::string_view>{}(
                std::string_view{
                    reinterpret_cast<const char*>(&tuple),
                    sizeof tuple
                });
        }

        template <typename ...Types>
        struct byte_hash
        {
            size_t operator()(const Types& ...args) noexcept {
                return hash_value_from(args);
            }
        };

        template <>
        struct byte_hash<void>
        {
            template <typename ...Types>
            size_t operator()(const Types& ...args) const noexcept {
                return hash_value_from(args...);
            }
        };
    }

    using v3::byte_hash;
    using v3::hash_value_from;

    template <typename Hash>
    struct deref_hash
    {
        // smart pointer or iterator
        template <typename Handle>
        size_t operator()(const Handle& handle) const {
            return Hash{}(*handle);
        }
    };

    template <typename Handle>
    decltype(auto) get_pointer(Handle&& handle,
                               std::enable_if_t<meta::has_operator_dereference<Handle>::value>* = nullptr) {
        return std::forward<Handle>(handle).operator->();
    }

    template <typename Pointee>
    Pointee* const& get_pointer(Pointee* const& handle) noexcept {
        return handle;
    }

    template <typename T>
    [[nodiscard]] constexpr std::remove_const_t<T>& as_mutable(T& object) noexcept {
        return const_cast<std::remove_const_t<T>&>(object);
    }

    template <typename T>
    [[nodiscard]] constexpr T& as_mutable(const T* ptr) noexcept {
        assert(ptr != nullptr);
        return const_cast<T&>(*ptr);
    }

    template <typename T>
    void as_mutable(const T&&) = delete;

    template <typename T, typename U>
    constexpr bool address_same(const T& x, const U& y) noexcept {
        return std::addressof(x) == std::addressof(y);
    }

    template <typename Enum>
    constexpr std::underlying_type_t<Enum> underlying(const Enum& enumeration) noexcept {
        static_assert(std::is_enum<Enum>::value);
        return static_cast<std::underlying_type_t<Enum>>(enumeration);
    }

    template <typename EnumT, typename EnumU>
    constexpr bool underlying_same(const EnumT& et, const EnumU& eu) noexcept {
        return std::equal_to<>{}(underlying(et), underlying(eu));
    }

    template <typename ...Types>
    struct overload final : Types...
    {
        using Types::operator()...;
    };

    template <typename ...Types>
    overload(Types ...) -> overload<Types...>;

    template <typename Variant, typename ...Callable>
    auto visit(Variant&& variant, Callable&& ...callable) {
        static_assert(meta::is_variant<std::decay_t<Variant>>::value);
        return std::visit(overload{ std::forward<Callable>(callable)... },
                          std::forward<Variant>(variant));
    }

    std::shared_ptr<folly::ThreadPoolExecutor> set_cpu_executor(int concurrency,
                                                                int queue_size,
                                                                std::string_view pool_name = "CorePool");

    std::shared_ptr<folly::ThreadPoolExecutor> set_cpu_executor(int concurrency,
                                                                std::string_view pool_name = "CorePool");

    std::shared_ptr<folly::ThreadedExecutor> make_threaded_executor(std::string_view thread_name = "CoreThread");

    std::shared_ptr<folly::ThreadPoolExecutor> make_pool_executor(int concurrency,
                                                                  int queue_size,
                                                                  bool throw_if_full,
                                                                  std::string_view pool_name = "CorePool");

    std::shared_ptr<folly::ThreadPoolExecutor> make_pool_executor(int concurrency,
                                                                  std::string_view pool_name = "CorePool");

    folly::Function<
        std::pair<int64_t, std::shared_ptr<spdlog::logger>>()>
    console_logger_factory(std::string logger_group);

    folly::Function<
        std::shared_ptr<spdlog::logger>&()>
    console_logger_access(std::string logger_name,
                          folly::Function<void(spdlog::logger&)> post_process = nullptr);
}
