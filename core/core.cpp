#include "stdafx.h"
#include "core.hpp"
#include <folly/executors/task_queue/UnboundedBlockingQueue.h>
#include <folly/executors/CPUThreadPoolExecutor.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <date/date.h>

namespace core
{
    std::string time_format(std::string_view format,
                            std::tm*(*timing)(std::time_t const*)) {
        // const auto time_tmt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        const auto current_time = std::time(nullptr);
        return fmt::format("{}", std::put_time(timing(&current_time), format.data()));
    }

    std::string date_format(std::string_view format) {
        return date::format(format.data(), std::chrono::system_clock::now());
    }

    size_t count_file_entry(const std::filesystem::path& directory) {
        // non-recursive version, regardless of symbolic link
        return std::distance(std::filesystem::directory_iterator{ directory },
                             std::filesystem::directory_iterator{});
    }

    std::pair<size_t, bool> make_empty_directory(const std::filesystem::path& directory) {
        assert(is_directory(directory.root_directory()));
        const auto remove_count = std::filesystem::remove_all(directory);
        const auto create_success = std::filesystem::create_directories(directory);
        return std::make_pair(folly::to<size_t>(remove_count),
                              create_success);
    }

    std::filesystem::path tidy_directory_path(const std::filesystem::path& directory) {
        return directory.has_filename()
                   ? directory
                   : directory.parent_path();
    }

    std::filesystem::path file_path_of_directory(const std::filesystem::path& directory,
                                                 const std::filesystem::path& extension) {
        assert(is_directory(directory));
        const auto file_name = tidy_directory_path(directory).filename();
        return (directory / file_name).replace_extension(extension);
    }

    std::shared_ptr<folly::ThreadPoolExecutor> set_cpu_executor(int concurrency,
                                                                int queue_size,
                                                                std::string_view pool_name) {
        static auto executor = make_pool_executor(concurrency, queue_size, false, pool_name);
        assert(executor->numThreads() == concurrency);
        folly::setCPUExecutor(executor);
        return executor;
    }

    std::shared_ptr<folly::ThreadPoolExecutor> set_cpu_executor(int concurrency,
                                                                std::string_view pool_name) {
        static auto executor = make_pool_executor(concurrency, pool_name);
        assert(executor->numThreads() == concurrency);
        folly::setCPUExecutor(executor);
        return executor;
    }

    std::shared_ptr<folly::ThreadedExecutor> make_threaded_executor(std::string_view thread_name) {
        return std::make_shared<folly::ThreadedExecutor>(
            std::make_shared<folly::NamedThreadFactory>(thread_name));
    }

    std::shared_ptr<folly::ThreadPoolExecutor> make_pool_executor(int concurrency,
                                                                  int queue_size,
                                                                  bool throw_if_full,
                                                                  std::string_view pool_name) {
        std::unique_ptr<folly::BlockingQueue<folly::CPUThreadPoolExecutor::CPUTask>> task_queue;
        if (throw_if_full) {
            task_queue = std::make_unique<
                folly::LifoSemMPMCQueue<
                    folly::CPUThreadPoolExecutor::CPUTask,
                    folly::QueueBehaviorIfFull::THROW>
            >(queue_size);
        } else {
            task_queue = std::make_unique<
                folly::LifoSemMPMCQueue<
                    folly::CPUThreadPoolExecutor::CPUTask,
                    folly::QueueBehaviorIfFull::BLOCK>
            >(queue_size);
        }
        return std::make_shared<folly::CPUThreadPoolExecutor>(
            std::make_pair(concurrency, 1),
            std::move(task_queue),
            std::make_shared<folly::NamedThreadFactory>(pool_name.data()));
    }

    std::shared_ptr<folly::ThreadPoolExecutor> make_pool_executor(int concurrency,
                                                                  std::string_view pool_name) {
        return std::make_shared<folly::CPUThreadPoolExecutor>(
            std::make_pair(concurrency, 1),
            std::make_unique<folly::UnboundedBlockingQueue<folly::CPUThreadPoolExecutor::CPUTask>>(),
            std::make_shared<folly::NamedThreadFactory>(pool_name.data()));
    }

    auto atomic_index = [](const int init = 0) {
        return std::make_unique<std::atomic<int64_t>>(init);
    };

    folly::Function<
        std::pair<int64_t, std::shared_ptr<spdlog::logger>
        >()
    >
    console_logger_factory(std::string logger_group) {

        return [logger_group = std::move(logger_group), indexer = atomic_index()] {
            const auto logger_index = indexer->fetch_add(1);
            const auto logger_name = fmt::format("{}${}", logger_group, logger_index);
            auto logger = spdlog::stdout_color_mt(logger_name);
            #ifdef NDEBUG
            logger->set_level(spdlog::level::info);
            #else
            logger->set_level(spdlog::level::debug);
            #endif
            return std::make_pair(logger_index, std::move(logger));
        };
    }

    folly::Function<
        std::shared_ptr<spdlog::logger>&()>
    console_logger_access(std::string logger_name,
                          folly::Function<void(spdlog::logger&)> post_process) {
        auto generate_logger = [logger_name, process = std::move(post_process)]() mutable {
            auto logger = spdlog::stdout_color_mt(logger_name);
            if (process != nullptr) {
                process(*logger);
            }
            return logger;
        };
        return [logger = folly::lazy(std::move(generate_logger))]() mutable -> decltype(auto) {
            return logger();
        };
    }
}
