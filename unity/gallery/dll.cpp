#include "stdafx.h"
#include "dll.h"

namespace
{
    std::atomic<int16_t> module_count = 0;
    std::shared_ptr<folly::CPUThreadPoolExecutor> cpu_thread_pool_executor;
    std::shared_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> io_context_guard;
    std::shared_ptr<boost::asio::io_context> io_context;
    std::mutex resouce_mutex;
}

namespace dll
{
    std::shared_ptr<folly::NamedThreadFactory> create_thread_factory(std::string_view name_prefix)
    {
        return std::make_shared<folly::NamedThreadFactory>(name_prefix.data());
    }

    folly::Function<void()> on_initialize_resouce(int task_queue_capacity)
    {
        return [task_queue_capacity]
        {
            auto const thread_capacity = std::thread::hardware_concurrency();
            cpu_thread_pool_executor = std::make_shared<folly::CPUThreadPoolExecutor>(
                thread_capacity,
                std::make_unique<folly::LifoSemMPMCQueue<
                    folly::CPUThreadPoolExecutor::CPUTask, folly::QueueBehaviorIfFull::THROW>>(task_queue_capacity),
                create_thread_factory("GalleryPool"));
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
            cpu_thread_pool_executor->stop();
            cpu_thread_pool_executor->join();
            //  cpu_thread_pool_executor.reset();
            io_context_guard->reset();
            io_context_guard.reset();
            io_context->stop();
            io_context.reset();
        };
    }

    int16_t register_module()
    {
        std::lock_guard<std::mutex> guard{ resouce_mutex };
        auto const module_index = std::atomic_fetch_add(&module_count, 1);
        if (module_index == 0)
            std::invoke(on_initialize_resouce(8'192));
        return module_index;
    }

    int16_t deregister_module()
    {
        std::lock_guard<std::mutex> guard{ resouce_mutex };
        auto const module_left = std::atomic_fetch_sub(&module_count, 1) - 1;
        if (module_left <= 0)
        {
            assert(module_left == 0);
            std::invoke(on_release_resouce());
        }
        return module_left;
    }

    folly::CPUThreadPoolExecutor& cpu_executor()
    {
        return cpu_thread_pool_executor.operator*();
    }

    boost::asio::io_context::executor_type asio_executor()
    {
        return io_context->get_executor();
    }
}
