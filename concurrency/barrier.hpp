#pragma once

namespace util
{
    inline namespace concurrency
    {
    #ifdef CORE_USE_LEGACY
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
    #endif
        namespace v3
        {
            template<typename Callable = void>
            class barrier;

            template<>
            class barrier<void> : core::noncopyable
            {
                std::atomic<std::ptrdiff_t> dropped_ = 0;
                std::atomic<std::ptrdiff_t> count_ = 0;
                std::ptrdiff_t threshold_ = 0;
                std::size_t generation_ = 0;
                boost::condition_variable mutable condition_;
                boost::mutex mutable mutex_;
            public:
                explicit barrier(std::ptrdiff_t const threshold)
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
                std::pair<std::ptrdiff_t, boost::unique_lock<boost::mutex>> count_down_and_lock()
                {
                    std::atomic_thread_fence(std::memory_order_release);
                    boost::unique_lock<boost::mutex> exlock{ mutex_ };
                    return std::make_pair(std::atomic_fetch_sub_explicit(&count_, 1, std::memory_order_relaxed), std::move(exlock));
                }

                void unlock_and_notify(boost::unique_lock<boost::mutex> exlock) const
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

                void wait_next_generation(boost::unique_lock<boost::mutex> exlock) const
                {
                    return condition_.wait(exlock, [this, gen = generation_] { return gen != generation_; });
                }
            };

            template<typename Callable>
            class barrier : barrier<>
            {
                using barrier<>::arrive_and_wait;
                std::decay_t<Callable> const completion_;
            public:
                barrier(std::ptrdiff_t const threshold, Callable&& call)
                    : barrier<>(threshold)
                    , completion_(std::move(call))
                {}

                barrier(std::ptrdiff_t const threshold, Callable const& call)
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
            };
        }

        using v3::barrier;
    }
}
