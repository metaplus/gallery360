#include "stdafx.h"
#include "async_chain.h"

void util::concurrency::async_chain::wait() const
{
    const auto pending_old = std::atomic_load_explicit(&pending_, std::memory_order_relaxed);
    if (pending_old) pending_old->wait();
}

void util::concurrency::async_chain::abort_and_wait() const
{
    canceled_.store(true, std::memory_order_seq_cst);
    wait();
}
