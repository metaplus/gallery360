#pragma once
#include "core/core.h"
#include "core/exception.hpp"
#include <leveldb/db.h>
#include <folly/MPMCQueue.h>
#include <folly/Lazy.h>
#include <boost/process/environment.hpp>
#include <chrono>

inline namespace plugin
{
    using std::chrono::operator ""ms;

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

        void wait_consume_cancel(bool timed) {
            active_ = false;
            if (!timed) {
                sink_entry_queue_.enqueue({ "", "" });
            }
            for (auto& consume_finish : consume_latch_) {
                consume_finish.wait();
            }
            consume_latch_.clear();
        }

        static std::shared_ptr<database> make_opened(std::string_view path) {
            auto database = std::make_shared<class database>();
            assert(is_directory(std::filesystem::path{ path }.root_directory()));
            core::as_mutable(database->directory_) = std::filesystem::path{ path };
            database->active_ = true;
            return database;
        }

    private:
        static std::unique_ptr<leveldb::DB> open_database(const std::string& path) {
            leveldb::DB* db = nullptr;
            leveldb::Options options;
            options.create_if_missing = true;
            auto status = leveldb::DB::Open(options, path, &db);
            assert(status.ok());
            assert(db != nullptr);
            return std::unique_ptr<leveldb::DB>{ db };
        }

        void timed_consume_entry() {
            while (std::atomic_load(&active_)) {
                const auto batch_begin_time = std::chrono::steady_clock::now();
                const auto batch_end_time = batch_begin_time + batch_sink_interval / 2;
                {
                    const auto database = open_database(directory_.string());
                    do {
                        auto stride_step = batch_stride;
                        std::pair<std::string, std::string> entry;
                        while (--stride_step && sink_entry_queue_.try_dequeue_until(entry, batch_end_time)) {
                            auto&[instance, event] = entry;
                            if (instance.empty()) {
                                core::stream_drained_error::throw_directly();
                            }
                            database->Put(leveldb::WriteOptions{}, instance.data(), event);
                        }
                    } while (std::chrono::steady_clock::now() < batch_end_time);
                }
                std::this_thread::sleep_until(batch_begin_time + batch_sink_interval);
            }
            auto database = folly::lazy([this] {
                return open_database(directory_.string());
            });
            while (!sink_entry_queue_.empty()) {
                std::pair<std::string, std::string> entry;
                sink_entry_queue_.dequeue(entry);
                database()->Put(leveldb::WriteOptions{}, entry.first.data(), entry.second);
            }
        }

        void block_consume_entry() {
            auto database = folly::lazy([this] {
                return open_database(directory_.string());
            });
            while (true) {
                std::pair<std::string, std::string> entry;
                sink_entry_queue_.dequeue(entry);
                if (entry.first.empty()) {
                    break;
                }
                database()->Put(leveldb::WriteOptions{}, entry.first.data(), entry.second);
            }
            assert(!std::atomic_load(&active_));
        }
    };
}
