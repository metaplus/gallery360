#include "stdafx.h"
#include "core.hpp"
#include <folly/executors/task_queue/UnboundedBlockingQueue.h>
#include <folly/executors/CPUThreadPoolExecutor.h>

namespace core
{
    std::string time_format(std::string format, std::tm *(*timing)(std::time_t const *)) {
        // auto const time_tmt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        auto const t = std::time(nullptr);
        return fmt::format("{}", std::put_time(timing(&t), format.data()));
    }

    size_t count_file_entry(std::filesystem::path const & directory) {
        // non-recursive version, regardless of symbolic link
        const std::filesystem::directory_iterator iterator{ directory };
        return std::distance(begin(iterator), end(iterator));
    }

    void set_cpu_executor(int concurrency, int queue_size, std::string_view pool_name) {
        static auto executor = std::make_shared<folly::CPUThreadPoolExecutor>(
            std::make_pair(concurrency, 1),
            std::make_unique<folly::LifoSemMPMCQueue<folly::CPUThreadPoolExecutor::CPUTask, folly::QueueBehaviorIfFull::BLOCK>>(queue_size),
            std::make_shared<folly::NamedThreadFactory>(pool_name.data()));
        folly::setCPUExecutor(executor);
    }

    void set_cpu_executor(int concurrency, std::string_view pool_name) {
        static auto executor = std::make_shared<folly::CPUThreadPoolExecutor>(
            std::make_pair(concurrency, 1),
            std::make_unique<folly::UnboundedBlockingQueue<folly::CPUThreadPoolExecutor::CPUTask>>(),
            std::make_shared<folly::NamedThreadFactory>(pool_name.data()));
        folly::setCPUExecutor(executor);
    }
}
