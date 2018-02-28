#pragma once
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
}