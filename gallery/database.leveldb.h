#pragma once
#include <leveldb/db.h>

inline namespace plugin
{
    class database final : public std::enable_shared_from_this<database>
    {
        folly::UMPMCQueue<std::pair<std::string, std::string>, true> sink_entry_queue_;
        std::atomic<bool> active_ = false;
        std::vector<folly::SemiFuture<folly::Unit>> consume_latch_;
        const std::filesystem::path directory_;
        static constexpr std::chrono::milliseconds batch_sink_interval = 300ms;
        static constexpr int batch_stride = 20;

    public:
        auto consume_task(const bool timed) {
            auto [promise_finish, future_finish] = folly::makePromiseContract<folly::Unit>();
            consume_latch_.push_back(std::move(future_finish));
            return [=, self = shared_from_this(), promise_finish = std::move(promise_finish)]() mutable {
                timed ? timed_consume_entry() : block_consume_entry();
                promise_finish.setValue();
            };
        }

        auto produce_callback() {
            return [this, self = shared_from_this()](std::string_view instance, std::string event) {
                if (std::atomic_load(&active_)) {
                    auto timed_instance = fmt::format("[time]{}[instance]{}", core::local_date_time(), instance);
                    sink_entry_queue_.enqueue(std::make_pair(std::move(timed_instance), std::move(event)));
                }
            };
        }

        void wait_consume_cancel(bool timed);
        static std::shared_ptr<database> make_opened(std::string_view path);

    private:
        static std::unique_ptr<leveldb::DB> open_database(const std::string& path);
        void timed_consume_entry();
        void block_consume_entry();
    };
}
