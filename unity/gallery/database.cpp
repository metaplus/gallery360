#include "stdafx.h"
#include "database.h"

inline namespace plugin
{
    void database::stop_consume() {
        active_ = false;
        for (auto& consume_finish : consume_latch_) {
            consume_finish.wait();
        }
        consume_latch_.clear();
    }

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
