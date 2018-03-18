#pragma once

namespace util
{
    inline namespace concurrency
    {
        class barrier {
        public:
            explicit barrier(std::size_t count, std::function<void()> func = nullptr);
            barrier(const barrier&) = delete;
            barrier(barrier&&) = delete;
            barrier& operator=(const barrier&) = delete;
            barrier& operator=(barrier&&) = delete;
            ~barrier() = default;
            void arrive_and_wait();
            explicit operator bool() const;
        private:
            std::size_t count_;
            std::size_t generation_;
            const std::size_t threshold_;
            const std::unique_ptr<std::function<void()>> callable_;
            std::mutex mutex_;
            std::condition_variable condition_;
        };
    }
}