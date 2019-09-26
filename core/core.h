#pragma once
#include <folly/executors/ThreadedExecutor.h>
#include <folly/executors/ThreadPoolExecutor.h>
#include <folly/futures/Promise.h>
#include <boost/asio/buffer.hpp>
#include <spdlog/logger.h>
#include <ctime>
#include <filesystem>
#include <optional>
#include <type_traits>
#include <variant>
#include "core/meta/type_trait.hpp"
#include "core/meta/meta.hpp"

namespace core
{
    template <typename BufferSequence, typename ...TailSequence>
    typename std::enable_if<
        boost::asio::is_const_buffer_sequence<
            decltype(std::declval<BufferSequence>().data())>::value,
        std::list<boost::asio::const_buffer>
    >::type
    split_buffer_sequence(BufferSequence&& sequence, TailSequence&& ...tails) {
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
    typename std::enable_if<
        std::is_base_of<std::default_delete<T>, Deleter>::value,
        std::shared_ptr<T>&>::type
    access(std::shared_ptr<T>& ptr, Deleter&& deleter, Args&&... args) {
        if (!ptr) {
            ptr = std::shared_ptr<T>{
                new T{ std::forward<Args>(args)... },
                std::forward<Deleter>(deleter)
            };
        }
        return ptr;
    }
    
    template <typename T, typename ...Args>
    std::optional<T>& access(std::optional<T>& option, Args&&... args) {
        return option
                   ? option
                   : option.emplace(std::forward<Args>(args)...);
    }

    // Format template "%Y-%m-%d %H:%M:%S"
    std::string time_format(std::string_view format = "%c",
                            std::tm*(*timing)(const std::time_t*) = &std::localtime);
    // Format template "%Y-%m-%d %H:%M:%S.%E*f"
    std::string local_date_time();
    std::string local_date_time(const std::string& format);

    namespace literals
    {
        using integer_literal = unsigned long long int;

        constexpr size_t operator""_kbyte(const integer_literal n) {
            return n * 1024;
        }

        constexpr size_t operator""_mbyte(const integer_literal n) {
            return n * 1024 * 1024;
        }
    }

    size_t count_file_entry(const std::filesystem::path& directory);

    std::pair<size_t, bool> make_empty_directory(const std::filesystem::path& directory);

    template <typename EntryPredicate>
    typename std::enable_if<
        std::is_invocable_r<bool, EntryPredicate,
                            std::filesystem::directory_entry>::value,
        std::vector<std::filesystem::path>
    >::type
    filter_directory_entry(const std::filesystem::path& directory,
                           const EntryPredicate& predicate) {
#ifdef __linux__
        return std::accumulate(
#else
        return std::reduce(
#endif
            std::filesystem::directory_iterator{ directory },
            std::filesystem::directory_iterator{},
            std::vector<std::filesystem::path>{},
            [&predicate](std::vector<std::filesystem::path> container,
                         const std::filesystem::directory_entry& entry) {
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
        bool operator()(const std::filesystem::path& left,
                        const std::filesystem::path& right) const;
        bool operator()(const std::filesystem::directory_entry& left,
                        const std::filesystem::directory_entry& right) const;
    };

    template <typename T>
    std::reference_wrapper<std::decay_t<T>>
    make_null_reference_wrapper() noexcept {
        static void* null_pointer = nullptr;
        return std::reference_wrapper<T>{
            *reinterpret_cast<std::add_pointer_t<std::decay_t<T>>&>(null_pointer)
        };
    }

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
    [[nodiscard]] constexpr typename std::remove_const<T>::type&
    as_mutable(T& reference) noexcept {
        return const_cast<typename std::remove_const<T>::type&>(reference);
    }

    template <bool ReturnReference = true, typename T>
    [[nodiscard]] constexpr typename std::conditional<ReturnReference, T&, T*>::type
    as_mutable(const T* pointer) noexcept {
        assert(pointer != nullptr);
        return const_cast<typename
            std::conditional<ReturnReference, T&, T*>::type>(pointer);
    }

    template <typename T>
    void as_mutable(const T&&) = delete;

    template <typename T, typename U>
    constexpr bool address_same(const T& x, const U& y) noexcept {
        return std::addressof(x) == std::addressof(y);
    }

    template <typename Enum>
    constexpr typename std::underlying_type<Enum>::type
    underlying(const Enum& enumeration) noexcept {
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

    template <typename ...Args, typename ...Callables>
    auto visit(std::variant<Args...>& variant, Callables&& ...callable) {
        return std::visit(overload{ std::forward<Callables>(callable)... },
                          std::move(variant));
    }

    std::shared_ptr<folly::ThreadPoolExecutor> set_cpu_executor(int concurrency, int queue_size,
                                                                std::string_view pool_name = "CorePool");

    std::shared_ptr<folly::ThreadPoolExecutor> set_cpu_executor(int concurrency,
                                                                std::string_view pool_name = "CorePool");

    std::shared_ptr<folly::ThreadedExecutor> make_threaded_executor(std::string_view thread_name = "CoreThread");

    std::shared_ptr<folly::ThreadPoolExecutor> make_pool_executor(int concurrency, int queue_size, bool throw_if_full,
                                                                  std::string_view pool_name = "CorePool");

    std::shared_ptr<folly::ThreadPoolExecutor> make_pool_executor(int concurrency,
                                                                  std::string_view pool_name = "CorePool");

    using logger_access = meta::accessor<spdlog::logger, true>::type;
    using logger_process = meta::processor<spdlog::logger>::type;

    folly::Function<std::pair<int64_t, logger_access>()>
    console_logger_factory(std::string logger_group, bool null = false);

    logger_access console_logger_access(std::string logger_name,
                                        logger_process post_process = nullptr);
    logger_access null_logger_access(std::string logger_name);

    std::shared_ptr<spdlog::logger> make_async_logger(std::string logger_name,
                                                      spdlog::sink_ptr sink);
}
