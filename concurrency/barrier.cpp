#include "stdafx.h"
#include "barrier.h"

util::concurrency::barrier::barrier(const std::size_t count, std::function<void()> func)
    : count_(count), generation_(0), threshold_(count)
    , callable_(!func ? nullptr : std::make_unique<std::function<void()>>(std::move(func)))
{}

void util::concurrency::barrier::arrive_and_wait()
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

util::concurrency::barrier::operator bool() const
{
    return callable_ && *callable_;
}