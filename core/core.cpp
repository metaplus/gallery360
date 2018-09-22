#include "stdafx.h"
#include "core.hpp"
#include <folly/executors/task_queue/LifoSemMPMCQueue.h>
#include <folly/executors/CPUThreadPoolExecutor.h>

namespace core
{
    void set_cpu_executor(int concurrency, int queue_size, std::string_view pool_name) {
        static auto executor = std::make_shared<folly::CPUThreadPoolExecutor>(
            std::make_pair(concurrency, 1),
            std::make_unique<folly::LifoSemMPMCQueue<folly::CPUThreadPoolExecutor::CPUTask, folly::QueueBehaviorIfFull::BLOCK>>(queue_size),
            std::make_shared<folly::NamedThreadFactory>(pool_name.data()));
        folly::setCPUExecutor(executor);
    }
}
