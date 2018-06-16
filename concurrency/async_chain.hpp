#pragma once

namespace util
{
    inline namespace concurrency
    {
        // thread-safe lock-free asynchronous task chain
        class async_chain : boost::noncopyable
        {
            std::shared_ptr<boost::future<void>> pending_ = nullptr;
            std::atomic<bool> mutable canceled_ = false;

        public:
            async_chain() = default;

            template<typename Callable>
            void append(Callable&& callable)
            {
                if (canceled_.load(std::memory_order_acquire)) return;
                boost::promise<boost::future<void>*> signal_promise;
                auto signal_sfuture = signal_promise.get_future().share();
                auto const pcallable = std::make_shared<std::decay_t<Callable>>(std::forward<Callable>(callable));
                decltype(pending_) pending_new = nullptr;
                static thread_local std::vector<decltype(pending_)> temporary;
                auto pending_old = std::atomic_load_explicit(&pending_, std::memory_order_relaxed);
                do
                {
                    pending_new = std::make_shared<decltype(pending_)::element_type>(
                        std::async([pending_old, signal_sfuture, pcallable]() mutable
                        {
                            signal_sfuture.wait();
                            if (pending_old.get() != signal_sfuture.get()) return;
                            try
                            {
                                if (pending_old) pending_old->get();    // aborted predecessor throws exception here
                                pcallable->operator()();
                            } catch (...)
                            {
                                if constexpr(meta::is_packaged_task_v<std::decay_t<decltype(*pcallable)>>)
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

            template<typename Callable>
            boost::future<std::invoke_result_t<std::decay_t<Callable>>> append(Callable&& callable, core::use_future_t)
            {
                // emplace from lambda or move construct std::packaged_task
                boost::packaged_task<std::invoke_result_t<std::decay_t<Callable>>()> task{ std::forward<Callable>(callable) };
                auto task_result = task.get_future();
                append(std::move(task));
                return task_result;
            }

            void wait() const
            {
                auto const pending_old = std::atomic_load_explicit(&pending_, std::memory_order_relaxed);
                if (pending_old) pending_old->wait();
            }

            void abort_and_wait() const
            {
                std::atomic_store_explicit(&canceled_, true, std::memory_order_seq_cst);
                wait();
            }
        };
    }
}