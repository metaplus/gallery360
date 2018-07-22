#include "stdafx.h"
#include "dll.h"

namespace
{
    std::atomic<int16_t> module_count = 0;
    std::shared_ptr<folly::NamedThreadFactory> thread_factory;
    std::shared_ptr<folly::CPUThreadPoolExecutor> cpu_thread_pool_executor;
    std::shared_ptr<folly::ThreadedExecutor> threaded_executor;
    std::shared_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> io_context_guard;
    std::shared_ptr<boost::asio::io_context> io_context;
}

namespace dll
{
    folly::Function<void()> on_initialize_resouce(int task_queue_capacity)
    {
        return [task_queue_capacity]
        {
            auto const thread_capacity = std::thread::hardware_concurrency();
            thread_factory = std::make_shared<folly::NamedThreadFactory>("GalleryThreadFactory");
            cpu_thread_pool_executor = std::make_shared<folly::CPUThreadPoolExecutor>(
                thread_capacity,
                std::make_unique<folly::LifoSemMPMCQueue<
                    folly::CPUThreadPoolExecutor::CPUTask, folly::QueueBehaviorIfFull::THROW>>(task_queue_capacity),
                thread_factory);
            threaded_executor = std::make_shared<folly::ThreadedExecutor>(thread_factory);
            folly::setCPUExecutor(cpu_thread_pool_executor);
            io_context = std::make_shared<boost::asio::io_context>();
            io_context_guard = std::make_shared<boost::asio::executor_work_guard<
                boost::asio::io_context::executor_type>>(io_context->get_executor());
        };
    }

    folly::Function<void()> on_release_resouce()
    {
        return []
        {
            cpu_thread_pool_executor.reset();
            threaded_executor.reset();
            io_context_guard->reset();
            io_context_guard.reset();
            io_context->stop();
            io_context.reset();
        };
    }

    int16_t register_module()
    {
        auto const module_index = std::atomic_fetch_add(&module_count, 1);
        static std::once_flag flag;
        std::call_once(flag, on_initialize_resouce(4'096));
        return module_index;
    }

    int16_t deregister_module()
    {
        auto const module_left = std::atomic_fetch_sub(&module_count, 1) - 1;
        if (module_left <= 0)
        {
            assert(module_left == 0);
            static std::once_flag flag;
            std::call_once(flag, on_release_resouce());
        }
        return module_left;
    }

    folly::CPUThreadPoolExecutor& cpu_executor()
    {
        return cpu_thread_pool_executor.operator*();
    }

    folly::ThreadedExecutor& thread_executor()
    {
        return threaded_executor.operator*();
    }

    boost::asio::io_context::executor_type asio_executor()
    {
        return io_context->get_executor();
    }
}
