#pragma once

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
        auto consume_task() {
            auto [promise_finish, future_finish] = folly::makePromiseContract<folly::Unit>();
            consume_latch_.push_back(std::move(future_finish));
            return [this, self = shared_from_this(), promise_finish = std::move(promise_finish)]() mutable {
                while (active_) {
                    auto batch_begin_time = std::chrono::steady_clock::now();
                    auto batch_end_time = batch_begin_time + batch_sink_interval / 2;
                    {
                        const auto database = open_database(directory_.string());
                        do {
                            auto stride_step = batch_stride;
                            std::pair<std::string, std::string> entry;
                            while (--stride_step && sink_entry_queue_.try_dequeue_until(entry, batch_end_time)) {
                                database->Put(leveldb::WriteOptions{},
                                              entry.first.data(), entry.second);
                            }
                        }
                        while (std::chrono::steady_clock::now() < batch_end_time);
                    }
                    std::this_thread::sleep_until(batch_begin_time + batch_sink_interval);
                }
                {
                    auto database = folly::lazy([this] {
                        return open_database(directory_.string());
                        });
                    while (!sink_entry_queue_.empty()) {
                        std::pair<std::string, std::string> entry;
                        sink_entry_queue_.dequeue(entry);
                        database()->Put(leveldb::WriteOptions{},
                            entry.first.data(), entry.second);
                    }
                }
                promise_finish.setValue();
            };
        }

        auto produce_callback() {
            return [this, self = shared_from_this()](std::string_view instance, std::string event) {
                if (active_) {
                    auto timed_instance = fmt::format("[time]{}[instance]{}", core::local_date_time(), instance);
                    sink_entry_queue_.enqueue(std::make_pair(std::move(timed_instance), std::move(event)));
                }
            };
        }

        void stop_consume();

        static std::shared_ptr<database> make_ptr(std::string_view path);

        static std::shared_ptr<database> make_ptr(bool open_or_create);

    private:
        static std::unique_ptr<leveldb::DB> open_database(const std::string& path);
    };
}
