#include "stdafx.h"
#include "core.hpp"
#include <folly/executors/task_queue/UnboundedBlockingQueue.h>
#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/executors/ThreadedExecutor.h>

namespace core
{
    std::string time_format(std::string format, std::tm *(*timing)(std::time_t const *)) {
        // const auto time_tmt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        const auto t = std::time(nullptr);
        return fmt::format("{}", std::put_time(timing(&t), format.data()));
    }

    size_t count_file_entry(const std::filesystem::path& directory) {
        // non-recursive version, regardless of symbolic link
        const std::filesystem::directory_iterator iterator{ directory };
        return std::distance(begin(iterator), end(iterator));
    }

    std::pair<size_t, bool> make_empty_directory(const std::filesystem::path & directory) {
        assert(is_directory(directory.root_directory()));
        const auto remove_count = std::filesystem::remove_all(directory);
        const auto create_success = std::filesystem::create_directories(directory);
        return std::make_pair(boost::numeric_cast<size_t>(remove_count), create_success);
    }

    std::shared_ptr<folly::ThreadPoolExecutor> set_cpu_executor(int concurrency, int queue_size, std::string_view pool_name) {
        static auto executor = make_pool_executor(concurrency, queue_size, false, pool_name);
        assert(executor->numThreads() == concurrency);
        folly::setCPUExecutor(executor);
        return executor;
    }

    std::shared_ptr<folly::ThreadPoolExecutor> set_cpu_executor(int concurrency, std::string_view pool_name) {
        static auto executor = make_pool_executor(concurrency, pool_name);
        assert(executor->numThreads() == concurrency);
        folly::setCPUExecutor(executor);
        return executor;
    }

    std::shared_ptr<folly::ThreadedExecutor> make_threaded_executor(std::string_view thread_name) {
        return std::make_shared<folly::ThreadedExecutor>(
            std::make_shared<folly::NamedThreadFactory>(thread_name));
    }

    std::shared_ptr<folly::ThreadPoolExecutor> make_pool_executor(int concurrency, int queue_size,
                                                                  bool throw_if_full, std::string_view pool_name) {
        std::unique_ptr<folly::BlockingQueue<folly::CPUThreadPoolExecutor::CPUTask>> task_queue;
        if (throw_if_full) {
            task_queue = std::make_unique<folly::LifoSemMPMCQueue<folly::CPUThreadPoolExecutor::CPUTask, folly::QueueBehaviorIfFull::THROW>>(queue_size);
        } else {
            task_queue = std::make_unique<folly::LifoSemMPMCQueue<folly::CPUThreadPoolExecutor::CPUTask, folly::QueueBehaviorIfFull::BLOCK>>(queue_size);
        }
        return std::make_shared<folly::CPUThreadPoolExecutor>(
            std::make_pair(concurrency, 1),
            std::move(task_queue),
            std::make_shared<folly::NamedThreadFactory>(pool_name.data()));
    }

    std::shared_ptr<folly::ThreadPoolExecutor> make_pool_executor(int concurrency, std::string_view pool_name) {
        return std::make_shared<folly::CPUThreadPoolExecutor>(
            std::make_pair(concurrency, 1),
            std::make_unique<folly::UnboundedBlockingQueue<folly::CPUThreadPoolExecutor::CPUTask>>(),
            std::make_shared<folly::NamedThreadFactory>(pool_name.data()));
    }
}
