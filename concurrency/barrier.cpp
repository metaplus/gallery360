#include "stdafx.h"
#include "barrier.hpp"

util::v1::barrier::barrier(const std::size_t count, std::function<void()> completion)
    : dropped_(0)
    , count_(count)
    , generation_(0)
    , threshold_(count)
    , completion_(!completion ? nullptr : std::make_unique<std::function<void()>>(std::move(completion)))
{}

void util::v1::barrier::arrive_and_wait()
{
    std::unique_lock<std::mutex> exlock{ mutex_ };
    const auto gen = generation_;
    if (1 == std::atomic_fetch_sub_explicit(&count_, 1, std::memory_order_acquire))
    {
        ++generation_;
        std::atomic_store_explicit(&count_,
            threshold_ - std::atomic_exchange_explicit(&dropped_, 0, std::memory_order_relaxed), std::memory_order_relaxed);
        if (completion_ && *completion_)
            (*completion_)();
        exlock.unlock();
        condition_.notify_all();
    }
    else condition_.wait(exlock, [this, gen] { return gen != generation_; });
}

void util::v1::barrier::arrive_and_drop()
{
    std::atomic_fetch_add_explicit(&dropped_, 1, std::memory_order_relaxed);
    std::atomic_fetch_sub_explicit(&count_, 1, std::memory_order_release);
}

util::v1::barrier::operator bool() const
{
    return completion_ && *completion_;
}

util::v1::barrier_once::barrier_once(const std::size_t count)
    : count_(count)
    , completion_([] { return; })
    , signal_(const_cast<std::packaged_task<void()>&>(completion_).get_future().share())
{}

util::v1::barrier_once::barrier_once(const std::size_t count, std::packaged_task<void()> completion)
    : count_(count)
    , completion_(std::move(completion))
    , signal_(const_cast<std::packaged_task<void()>&>(completion_).get_future().share())
{}

void util::v1::barrier_once::arrive_and_wait()
{
    if (1 == std::atomic_fetch_sub_explicit(&count_, 1, std::memory_order_acquire))
        const_cast<std::packaged_task<void()>&>(completion_)();
    else signal_.get();
}

void util::v1::barrier_once::arrive_and_drop()
{
    std::atomic_fetch_sub_explicit(&count_, 1, std::memory_order_release);
}