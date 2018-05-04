#pragma once

namespace util
{
    inline namespace concurrency
    {
        class barrier 
        {
        public:
            explicit barrier(std::size_t count, std::function<void()> completion = nullptr);
            barrier(const barrier&) = delete;
            barrier(barrier&&) = delete;
            barrier& operator=(const barrier&) = delete;
            barrier& operator=(barrier&&) = delete;
            ~barrier() = default;
            void arrive_and_wait();
            void arrive_and_drop();
            explicit operator bool() const;
        private:
            std::atomic<size_t> dropped_;
            std::atomic<size_t> count_;
            std::size_t generation_;
            const std::size_t threshold_;
            const std::unique_ptr<std::function<void()>> completion_;   // 8B pointer 64B pointee, std::function may be less efficient than lambda-expression, depending on vendor implementation
            mutable std::mutex mutex_;
            mutable std::condition_variable condition_;
        };
        
        class barrier_once
        {
        public:
            explicit barrier_once(std::size_t count);
            barrier_once(std::size_t count, std::packaged_task<void()> completion);
            barrier_once(const barrier_once&) = delete;
            barrier_once(barrier_once&&) = delete;
            barrier_once& operator=(const barrier_once&) = delete;
            barrier_once& operator=(barrier_once&&) = delete;
            ~barrier_once() = default;
            void arrive_and_wait();
            void arrive_and_drop();
        private:
            std::atomic<size_t> count_;
            const std::packaged_task<void()> completion_;
            const std::shared_future<void> signal_;
        };
    }
}