#pragma once
#include "verify.hpp"

namespace sync
{
    class [[deprecated]] spin_mutex
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

    // non-atomic lock upgrading
    template<typename Mutex>
    std::unique_lock<Mutex> lock_upgrade(std::shared_lock<Mutex>& shared_lock)
    {
        core::verify(shared_lock.owns_lock());
        auto pmutex = shared_lock.release();
        pmutex.unlock_shared();
        return std::unique_lock<Mutex>{ *pmutex };
    }
    
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
            if (canceled_.load(std::memory_order_acquire)) return;
            std::promise<decltype(pending_)::element_type*> promise;
            auto sfuture = promise.get_future().share();
            decltype(pending_) pending_new = nullptr;
            static thread_local std::vector<decltype(pending_)> temporary;
            auto pending_old = std::atomic_load_explicit(&pending_, std::memory_order_relaxed);
			do
			{
				pending_new = std::make_shared<decltype(pending_)::element_type>(
					std::async([pending_old, sfuture, callable = std::forward<Callable>(callable)]() mutable
				{
					sfuture.wait();
                    if (pending_old.get() != sfuture.get()) return;
                    if (pending_old) pending_old->get(); 
					std::invoke(callable);
				}));
				temporary.push_back(pending_new);
            } while (!std::atomic_compare_exchange_strong_explicit(&pending_, &pending_old, pending_new,
                std::memory_order_acq_rel, std::memory_order_relaxed));
			promise.set_value(pending_old.get());
            temporary.clear();
		}
        void wait() const
		{
            const auto pending_old = std::atomic_load_explicit(&pending_, std::memory_order_acquire);
            if (pending_old) pending_old->wait();
		}
        void abort_and_wait()
		{
            canceled_.store(true, std::memory_order_seq_cst);
            append([] { throw core::force_exit_exception{}; });
            wait();
		}
	};
}