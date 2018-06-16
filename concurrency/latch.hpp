#pragma once

namespace util
{
    inline namespace concurrency
    {
        class latch : core::noncopyable
        {
            std::atomic<std::ptrdiff_t> count_ = 0;
            boost::promise<void> mutable completion_;
            boost::future<void> mutable signal_;

        public:
            explicit latch(std::ptrdiff_t const count)
                : count_(count)
                , signal_(completion_.get_future())
            {}

            void count_down_and_wait()
            {
                if (std::atomic_fetch_sub_explicit(&count_, 1, std::memory_order_relaxed) > 1)
                    return signal_.wait();
                completion_.set_value();
            }

            void count_down(const std::ptrdiff_t n = 1)
            {
                std::atomic_fetch_sub_explicit(&count_, n, std::memory_order_release);
            }

            bool is_ready() const noexcept
            {
                if (std::atomic_load_explicit(&count_, std::memory_order_relaxed) > 0) return false;
                std::atomic_thread_fence(std::memory_order_acquire);
                return true;
            }

            void wait() const
            {
                signal_.wait();
            }
        };
    }
}