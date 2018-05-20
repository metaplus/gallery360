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
                generation_ += 1;
                threshold_ -= std::atomic_exchange_explicit(&dropped_, 0, std::memory_order_relaxed), std::memory_order_relaxed;
                std::atomic_store_explicit(&count_, threshold_, std::memory_order_relaxed);
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
            std::ptrdiff_t threshold_ = 0;
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
                generation_ += 1;
                threshold_ -= std::atomic_exchange_explicit(&dropped_, 0, std::memory_order_relaxed), std::memory_order_relaxed;
                std::atomic_store_explicit(&count_, threshold_, std::memory_order_relaxed);
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
            std::ptrdiff_t threshold_ = 0;
            mutable std::mutex mutex_;
            mutable std::condition_variable condition_;
        };
    }

    // using v2::barrier;

    namespace v3
    {
        template<typename Callable = void>
        class barrier;

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
                auto[prev_count, exlock] = count_down_and_lock();
                if (prev_count > 1) return wait_next_generation(std::move(exlock));
                turn_next_generation();
                unlock_and_notify(std::move(exlock));
            }

            void arrive_and_drop() noexcept
            {
                std::atomic_fetch_add_explicit(&dropped_, 1, std::memory_order_relaxed);
                std::atomic_fetch_sub_explicit(&count_, 1, std::memory_order_release);
            }

        protected:
            std::pair<std::ptrdiff_t, std::unique_lock<std::mutex>> count_down_and_lock() 
            {
                std::atomic_thread_fence(std::memory_order_release);
                std::unique_lock<std::mutex> exlock{ mutex_ };
                return std::make_pair(std::atomic_fetch_sub_explicit(&count_, 1, std::memory_order_relaxed), std::move(exlock));
            }

            void unlock_and_notify(std::unique_lock<std::mutex> exlock) const
            {
                exlock.unlock();
                condition_.notify_all();
            }

            void turn_next_generation() noexcept
            {
                std::atomic_thread_fence(std::memory_order_acquire);
                generation_ += 1;
                threshold_ -= std::atomic_exchange_explicit(&dropped_, 0, std::memory_order_relaxed);
                std::atomic_store_explicit(&count_, threshold_, std::memory_order_relaxed);
            }

            void wait_next_generation(std::unique_lock<std::mutex> exlock) const
            {
                return condition_.wait(exlock, [this, gen = generation_] { return gen != generation_; });
            }

        private:
            std::atomic<std::ptrdiff_t> dropped_ = 0;
            std::atomic<std::ptrdiff_t> count_ = 0;
            std::ptrdiff_t threshold_ = 0;
            std::size_t generation_ = 0;
            mutable std::condition_variable condition_;
            mutable std::mutex mutex_;
        };

        template<typename Callable>
        class barrier : barrier<>
        {
        public:
            barrier(const std::ptrdiff_t threshold, Callable&& call)
                : barrier<>(threshold)
                , completion_(std::move(call))
            {}

            barrier(const std::ptrdiff_t threshold, const Callable& call)
                : barrier<>(threshold)
                , completion_(call)
            {}

            void arrive_and_wait()
            {
                auto[prev_count, exlock] = count_down_and_lock();
                if (prev_count > 1) return wait_next_generation(std::move(exlock));
                turn_next_generation();
                std::invoke(completion_);
                unlock_and_notify(std::move(exlock));
            }

            using barrier<>::arrive_and_drop;

        private:
            using barrier<>::arrive_and_wait;

            const std::decay_t<Callable> completion_;
        };
    }

    using v3::barrier;
}