#include "stdafx.h"
#include "Core/sync.h"

#pragma warning(push)
#pragma warning(disable:4996)
sync::spin_mutex::spin_mutex()
    : flag_{ ATOMIC_FLAG_INIT }
{}

void sync::spin_mutex::lock()
{
    while (flag_.test_and_set(std::memory_order_acquire));
}

void sync::spin_mutex::unlock()
{
    flag_.clear(std::memory_order_release);
}
#pragma warning(pop)

sync::barrier::barrier(const std::size_t count, std::function<void()> func)
    : count_(count), generation_(0), threshold_(count)
    , callable_(!func ? nullptr : std::make_unique<std::function<void()>>(std::move(func)))
{}

void sync::barrier::arrive_and_wait()
{
    std::unique_lock<std::mutex> exlock{ mutex_ };
    auto gen = generation_;
    if (!--count_)
    {
        ++generation_;
        count_ = threshold_;
        if (callable_ && *callable_)
            (*callable_)();
        exlock.unlock();
        condition_.notify_all();
    }
    else condition_.wait(exlock, [this, gen] { return gen != generation_; });
}

sync::barrier::operator bool() const
{
    return callable_ && *callable_;
}

void sync::chain::wait() const
{
    const auto pending_old = std::atomic_load_explicit(&pending_, std::memory_order_relaxed);
    if (pending_old) pending_old->wait();
}

void sync::chain::abort_and_wait()
{
    canceled_.store(true, std::memory_order_seq_cst);
    wait();
}
