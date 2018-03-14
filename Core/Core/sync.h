#pragma once

namespace sync
{
    // non-atomic lock upgrading
    template<typename Mutex>
    std::unique_lock<Mutex> lock_upgrade(std::shared_lock<Mutex>& shared_lock)
    {
        core::verify(shared_lock.owns_lock());
        auto pmutex = shared_lock.release();
        pmutex.unlock_shared();
        return std::unique_lock<Mutex>{ *pmutex };
    }

    class[[deprecated]] spin_mutex
    {
    public:
        spin_mutex();
        void lock();
        void unlock();
    private:
        std::atomic_flag flag_;
    };

    class barrier {
    public:
        explicit barrier(std::size_t count, std::function<void()> func = nullptr);
        barrier(const barrier&) = delete;
        barrier& operator=(const barrier&) = delete;
        void arrive_and_wait();
        explicit operator bool() const;
    private:
        std::size_t count_;
        std::size_t generation_;
        const std::size_t threshold_;
        const std::unique_ptr<std::function<void()>> callable_;
        std::mutex mutex_;
        std::condition_variable condition_;
    };

    struct use_future_t {};                                 // tag dispatch for future overload
    inline constexpr use_future_t use_future{};

    // thread-safe lock-free asynchronous task chain
    class chain
    {
    public:
        chain() = default;
        chain(const chain&) = delete;
        chain& operator=(const chain&) = delete;
        template<typename Callable>
        void append(Callable&& callable);
        template<typename Callable>
        std::future<std::invoke_result_t<Callable>> append(Callable&& callable, use_future_t);
        void wait() const;
        void abort_and_wait();
    private:
        std::shared_ptr<std::future<void>> pending_ = nullptr;
        std::atomic<bool> canceled_ = false;
    };

    template <typename Callable>
    void chain::append(Callable&& callable)
    {
        if (canceled_.load(std::memory_order_acquire))
            return;
        std::promise<decltype(pending_)::element_type*> signal_promise;
        auto signal_sfuture = signal_promise.get_future().share();
        const auto pcallable = std::make_shared<meta::remove_cv_ref_t<Callable>>(std::forward<Callable>(callable));
        decltype(pending_) pending_new = nullptr;
        static thread_local std::vector<decltype(pending_)> temporary;
        auto pending_old = std::atomic_load_explicit(&pending_, std::memory_order_relaxed);
        do
        {
            pending_new = std::make_shared<decltype(pending_)::element_type>(
                std::async([pending_old, signal_sfuture, pcallable]() mutable
            {
                signal_sfuture.wait();
                if (pending_old.get() != signal_sfuture.get())
                    return;
                try
                {
                    if (pending_old)
                        pending_old->get();                 // aborted predecessor throws exception here
                    (*pcallable)();
                }
                catch (...)
                {
                    if constexpr(meta::is_packaged_task_v<meta::remove_cv_ref_t<decltype(*pcallable)>>)
                        const auto abolished_task = std::move(*pcallable);
                    std::rethrow_exception(std::current_exception());
                }
            }));
            temporary.push_back(pending_new);
        } while (!std::atomic_compare_exchange_strong_explicit(&pending_, &pending_old, pending_new,
            std::memory_order_release, std::memory_order_relaxed));
        signal_promise.set_value(pending_old.get());
        temporary.clear();
    }

    template <typename Callable>
    std::future<std::invoke_result_t<Callable>> chain::append(Callable&& callable, use_future_t)
    {   // emplace from lambda or move construct std::packaged_task
        std::packaged_task<std::invoke_result_t<Callable>()> task{ std::forward<Callable>(callable) };
        auto task_result = task.get_future();
        append(std::move(task));
        return task_result;
    }
}
