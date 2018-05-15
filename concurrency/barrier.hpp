#pragma once

namespace util
{
    inline namespace concurrency
    {
        namespace v1
        {
            class barrier 
            {
            public:
                explicit barrier(std::size_t count, std::function<void()> completion = nullptr);
                barrier(const barrier&) = delete;
                barrier(barrier&&) = delete;
                barrier& operator=(const barrier&) = delete;
                barrier& operator=(barrier&&) = delete;
                ~barrier() = default;
                void arrive_and_wait();
                void arrive_and_drop();
                explicit operator bool() const;
            private:
                std::atomic<size_t> dropped_;
                std::atomic<size_t> count_;
                std::size_t generation_;
                const std::size_t threshold_;
                const std::unique_ptr<std::function<void()>> completion_;   // 8B pointer 64B pointee
                mutable std::mutex mutex_;
                mutable std::condition_variable condition_;
            };
        
            class barrier_once
            {
            public:
                explicit barrier_once(std::size_t count);
                barrier_once(std::size_t count, std::packaged_task<void()> completion);
                barrier_once(const barrier_once&) = delete;
                barrier_once(barrier_once&&) = delete;
                barrier_once& operator=(const barrier_once&) = delete;
                barrier_once& operator=(barrier_once&&) = delete;
                ~barrier_once() = default;
                void arrive_and_wait();
                void arrive_and_drop();
            private:
                std::atomic<size_t> count_;
                const std::packaged_task<void()> completion_;
                const std::shared_future<void> signal_;
            };
        }
    }

    // using v1::barrier;
    // using v1::barrier_once;

    namespace v2
    {
        template<typename Callable = void>
        class barrier : core::noncopyable
        {
        public:
            barrier(const std::ptrdiff_t threshold, Callable&& call)
                : count_(threshold)
                , threshold_(threshold)
                , completion_(std::move(call))
            {}

            void arrive_and_wait()
            {
                std::atomic_thread_fence(std::memory_order_release);
                std::unique_lock<std::mutex> exlock{ mutex_ };
                if (std::atomic_fetch_sub_explicit(&count_, 1, std::memory_order_relaxed) > 1)
                    return condition_.wait(exlock, [this, gen = generation_] { return gen != generation_; });
                std::atomic_thread_fence(std::memory_order_acquire);
                ++generation_;
                const auto threshold_next = threshold_ - std::atomic_exchange_explicit(&dropped_, 0, std::memory_order_relaxed);
                std::atomic_store_explicit(&count_, threshold_next, std::memory_order_relaxed);
                std::invoke(completion_);
                exlock.unlock();
                condition_.notify_all();
            }

            void arrive_and_drop()
            {
                std::atomic_fetch_add_explicit(&dropped_, 1, std::memory_order_relaxed);
                std::atomic_fetch_sub_explicit(&count_, 1, std::memory_order_release);
            }

        private:
            std::atomic<std::ptrdiff_t> dropped_ = 0;
            std::atomic<std::ptrdiff_t> count_ = 0;
            std::size_t generation_ = 0;
            const std::ptrdiff_t threshold_ = 0;
            const std::decay_t<Callable> completion_;
            mutable std::mutex mutex_;
            mutable std::condition_variable condition_;
        };

        template<>
        class barrier<void> : core::noncopyable
        {
        public:
            explicit barrier(const std::ptrdiff_t threshold)
                : count_(threshold)
                , threshold_(threshold)
            {}

            void arrive_and_wait()
            {
                std::atomic_thread_fence(std::memory_order_release);
                std::unique_lock<std::mutex> exlock{ mutex_ };
                if (std::atomic_fetch_sub_explicit(&count_, 1, std::memory_order_relaxed) > 1)
                    return condition_.wait(exlock, [this, gen = generation_] { return gen != generation_; });
                std::atomic_thread_fence(std::memory_order_acquire);
                ++generation_;
                const auto threshold_next = threshold_ - std::atomic_exchange_explicit(&dropped_, 0, std::memory_order_relaxed);
                std::atomic_store_explicit(&count_, threshold_next, std::memory_order_relaxed);
                exlock.unlock();
                condition_.notify_all();
            }

            void arrive_and_drop()
            {
                std::atomic_fetch_add_explicit(&dropped_, 1, std::memory_order_relaxed);
                std::atomic_fetch_sub_explicit(&count_, 1, std::memory_order_release);
            }

        private:
            std::atomic<std::ptrdiff_t> dropped_ = 0;
            std::atomic<std::ptrdiff_t> count_ = 0;
            std::size_t generation_ = 0;
            const std::ptrdiff_t threshold_ = 0;
            mutable std::mutex mutex_;
            mutable std::condition_variable condition_;
        };
    }

    using v2::barrier;
}