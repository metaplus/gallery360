#pragma once

namespace util
{
    inline namespace concurrency
    {
        // non-atomic lock upgrading
        template<typename Mutex>
        std::unique_lock<Mutex> lock_upgrade(std::shared_lock<Mutex>& shared_lock)
        {
            core::verify(shared_lock.owns_lock());
            auto pmutex = shared_lock.release();
            pmutex->unlock_shared();
            return std::unique_lock<Mutex>{ *pmutex };
        }

        // non-atomic lock downgrading
        template<typename Mutex>
        std::shared_lock<Mutex> lock_downgrade(std::unique_lock<Mutex>& exclusive_lock)
        {
            core::verify(exclusive_lock.owns_lock());
            auto pmutex = exclusive_lock.release();
            pmutex->unlock();
            return std::shared_lock<Mutex>{ *pmutex };
        }

        class[[deprecated]] spin_mutex
        {
            std::atomic_flag flag_;

        public:
        #pragma warning(push)
        #pragma warning(disable:4996)
            spin_mutex()
                : flag_{ ATOMIC_FLAG_INIT }
            {}

            void lock()
            {
                while (std::atomic_flag_test_and_set_explicit(&flag_, std::memory_order_acquire));
            }

            void unlock()
            {
                std::atomic_flag_clear_explicit(&flag_, std::memory_order_release);
            }
        #pragma warning(pop)
        };
    }
}