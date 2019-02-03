#include "stdafx.h"
#include "database.leveldb.h"
#include <boost/process/environment.hpp>

inline namespace plugin
{
    void database::wait_consume_cancel(const bool timed) {
        active_ = false;
        if (!timed) {
            sink_entry_queue_.enqueue({ "", "" });
        }
        for (auto& consume_finish : consume_latch_) {
            consume_finish.wait();
        }
        consume_latch_.clear();
    }

    auto latest_database = [](const std::filesystem::path& path) {
        auto database_iterator = std::filesystem::directory_iterator{ path };
        auto end_iterator = std::filesystem::directory_iterator{};
        assert(database_iterator != end_iterator);
        using entry_reference = std::filesystem::directory_iterator::reference;
        database_iterator = std::max_element(
            std::execution::par, database_iterator, end_iterator,
            [](entry_reference left, entry_reference right) {
                return left.last_write_time() < right.last_write_time();
            });
        return database_iterator->path();
    };

    std::shared_ptr<database> database::make_opened(std::string_view path) {
        auto database = std::make_shared<class database>();
        assert(is_directory(std::filesystem::path{ path }.root_directory()));
        core::as_mutable(database->directory_) = std::filesystem::path{ path };
        database->active_ = true;
        return database;
    }

    std::unique_ptr<leveldb::DB> database::open_database(const std::string& path) {
        leveldb::DB* db = nullptr;
        leveldb::Options options;
        options.create_if_missing = true;
        auto status = leveldb::DB::Open(options, path, &db);
        assert(status.ok());
        assert(db != nullptr);
        return std::unique_ptr<leveldb::DB>{ db };
    }

    void database::timed_consume_entry() {
        while (std::atomic_load(&active_)) {
            const auto batch_begin_time = std::chrono::steady_clock::now();
            const auto batch_end_time = batch_begin_time + batch_sink_interval / 2;
            {
                const auto database = open_database(directory_.string());
                do {
                    auto stride_step = batch_stride;
                    std::pair<std::string, std::string> entry;
                    while (--stride_step && sink_entry_queue_.try_dequeue_until(entry, batch_end_time)) {
                        auto& [instance, event] = entry;
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

    void database::block_consume_entry() {
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
}
