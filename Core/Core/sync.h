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
        std::atomic_flag flag_;
    public:
        spin_mutex() :
            flag_{ ATOMIC_FLAG_INIT }
        {}
#pragma warning(push)
#pragma warning(disable:4996)
        void lock()
        {
            while (flag_.test_and_set(std::memory_order_acquire));
        }
        void unlock()
        {
            flag_.clear(std::memory_order_release);
        }
#pragma warning(pop)
    };
    
    template<typename Callable, typename ...Args>
    void set_promise_through(std::promise<std::invoke_result_t<Callable>>& promise, Callable&& callable, Args&& ...args)
    {
        try
        {
            if constexpr(std::is_same_v<std::invoke_result_t<Callable>, void>)
            {
                std::invoke(std::forward<Callable>(callable), std::forward<Args>(args)...);
                promise.set_value();
            }
            else
                promise.set_value(std::invoke(std::forward<Callable>(callable), std::forward<Args>(args)...));
        }
        catch (...)
        {
            promise.set_exception(std::current_exception());
        }        
    }

    struct use_future_t {};                                 // tag dispatch for future overload
    inline constexpr use_future_t use_future{};
    // thread-safe lock-free asynchronous task chain
    class chain
    {
        std::shared_ptr<std::future<void>> pending_;
        std::atomic<bool> canceled_ = false;
    public:
        chain() = default;
        chain(const chain&) = delete;
        chain& operator=(const chain&) = delete;
        template<typename Callable>
        void append(Callable&& callable)
        {
            if (canceled_.load(std::memory_order_acquire)) 
                return;
            std::promise<decltype(pending_)::element_type*> signal_promise;
            auto signal_sfuture = signal_promise.get_future().share();
            decltype(pending_) pending_new = nullptr;
            static thread_local std::vector<decltype(pending_)> temporary;
            auto pending_old = std::atomic_load_explicit(&pending_, std::memory_order_relaxed);
            do
            {
                pending_new = std::make_shared<decltype(pending_)::element_type>(
                    std::async([pending_old, signal_sfuture, callable = std::forward<Callable>(callable)]() mutable
                {
                    signal_sfuture.wait();
                    if (pending_old.get() != signal_sfuture.get()) 
                        return;
                    if (pending_old) 
                        pending_old->get();                 // aborted predecessor throws exception here
                    std::invoke(callable);
                }));
                temporary.push_back(pending_new);
            } while (!std::atomic_compare_exchange_strong_explicit(&pending_, &pending_old, pending_new,
                std::memory_order_acq_rel, std::memory_order_relaxed));
            signal_promise.set_value(pending_old.get());
            temporary.clear();
        }
        template<typename Callable>
        std::future<std::invoke_result_t<Callable>> append(Callable&& callable, use_future_t)
        {
            if (canceled_.load(std::memory_order_acquire)) 
                return {};
            std::promise<decltype(pending_)::element_type*> signal_promise;
            auto signal_sfuture = signal_promise.get_future().share();
            decltype(pending_) pending_new = nullptr;
            auto pending_result_promise = std::make_shared<std::promise<std::invoke_result_t<Callable>>>();
            auto pending_result_future = pending_result_promise->get_future();
            static thread_local std::vector<decltype(pending_)> temporary;
            auto pending_old = std::atomic_load_explicit(&pending_, std::memory_order_relaxed);
            do
            {
                pending_new = std::make_shared<decltype(pending_)::element_type>(
                    std::async([pending_old, pending_result_promise, signal_sfuture, callable = std::forward<Callable>(callable)]() mutable
                {
                    signal_sfuture.wait();
                    if (pending_old.get() != signal_sfuture.get()) return;
                    try
                    {
                        if (pending_old) 
                            pending_old->get();             // aborted predecessor throws exception here
                        set_promise_through(*pending_result_promise, callable);
                    }
                    catch (...)
                    {
                        pending_result_promise->set_exception(std::current_exception());
                    }
                }));
                temporary.push_back(pending_new);
            } while (!std::atomic_compare_exchange_strong_explicit(&pending_, &pending_old, pending_new,
                std::memory_order_acq_rel, std::memory_order_relaxed));
            signal_promise.set_value(pending_old.get());
            temporary.clear();
            return pending_result_future;
        }
        void wait() const
        {
            const auto pending_old = std::atomic_load_explicit(&pending_, std::memory_order_relaxed);
            if (pending_old) pending_old->wait();
        }
        void abort_and_wait()
        {
            canceled_.store(true, std::memory_order_seq_cst);
            wait();
        }
    };
}