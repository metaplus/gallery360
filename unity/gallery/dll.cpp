#include "stdafx.h"
#include "dll.h"

namespace
{
    std::atomic<int16_t> module_count = 0;
    boost::asio::io_context io_context;
    boost::thread_group thread_group;
    folly::Synchronized<std::optional<net::executor_guard>> executor_guard;
    folly::small_vector<boost::thread*, 8, folly::small_vector_policy::NoHeap> threads;
}
namespace dll
{
    boost::asio::io_context& register_module()
    {
        std::atomic_fetch_add(&module_count, 1);
        auto ulock_guard = executor_guard.ulock();
        if (!ulock_guard->has_value())
        {
            auto wlock_guard = ulock_guard.moveFromUpgradeToWrite();
            wlock_guard->emplace(thread_group, io_context);
            std::generate_n(std::back_inserter(threads), boost::thread::hardware_concurrency(),
                            [] { return thread_group.create_thread([] { io_context.run(); }); });
            ulock_guard = wlock_guard.moveFromWriteToUpgrade();
        }
        return io_context;
    }

    int16_t unregister_module()
    {
        auto const module_left = std::atomic_fetch_sub(&module_count, 1);
        if (module_left == 1)
        {
            auto wlock_guard = executor_guard.wlock();
            io_context.stop();
            wlock_guard->reset();
            threads.clear();
        }
        return module_left;
    }
}
