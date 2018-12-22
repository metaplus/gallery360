#include "stdafx.h"
#include "database.h"
#include <boost/process/environment.hpp>

inline namespace plugin
{
    void database::wait_consume_stop() {
        active_ = false;
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

    std::shared_ptr<database> database::make_ptr(std::string_view path) {
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
}
