#include "stdafx.h"
#include "concurrency/synchronize.h"

#pragma warning(push)
#pragma warning(disable:4996)

util::concurrency::spin_mutex::spin_mutex()
    : flag_{ ATOMIC_FLAG_INIT }
{}

void util::concurrency::spin_mutex::lock()
{
    while (flag_.test_and_set(std::memory_order_acquire));
}

void util::concurrency::spin_mutex::unlock()
{
    flag_.clear(std::memory_order_release);
}

#pragma warning(pop)