#include "pch.h"
#include <leveldb/db.h>
#include <boost/process/environment.hpp>
#include <re2/re2.h>

namespace leveldb::test
{
    TEST(Database, Base) {
        leveldb::DB* db;
        leveldb::Options options;
        options.create_if_missing = true;
        auto status = leveldb::DB::Open(options, "F:/Debug/LogDb", &db);
        EXPECT_TRUE(status.ok());
        std::string value = "value";
        status = db->Put(leveldb::WriteOptions{}, "key1", value);
        EXPECT_TRUE(status.ok());
        value.clear();
        EXPECT_TRUE(value.empty());
        status = db->Get(leveldb::ReadOptions{}, "key1", &value);
        EXPECT_TRUE(status.ok());
        EXPECT_EQ(value, "value");
        status = db->Delete(leveldb::WriteOptions{}, "key1");
        EXPECT_TRUE(status.ok());
        delete db;
    }

    auto guarded_database(std::optional<std::string> path = std::nullopt) {
        auto guarded_database = std::make_unique<std::pair<leveldb::DB*, std::any>>(nullptr, std::any{});
        return folly::lazy([=, guarded_database = std::move(guarded_database)] {
            auto& [database,guard] = *guarded_database;
            if (!database) {
                leveldb::Options options;
                options.create_if_missing = false;
                auto status = leveldb::DB::Open(options, path.value_or("F:/Debug/TraceDb"), &database);
                EXPECT_TRUE(status.ok());
                EXPECT_TRUE(database != nullptr);
            }
            return (database);
        });
    }

    TEST(Database, Iterate) {
        auto database = guarded_database();
        auto* iterator = database()->NewIterator(leveldb::ReadOptions{});
        for (iterator->SeekToFirst(); iterator->Valid(); iterator->Next()) {
            XLOG(INFO) << fmt::format("key {} value {}", iterator->key().ToString(), iterator->value().ToString());
        }
    }

    TEST(Database, IterateWorkset) {
        const auto workset_env = boost::this_process::environment()["GWorkSet"];
        ASSERT_FALSE(workset_env.empty());
        const std::filesystem::path workset_directory = workset_env.to_string();
        ASSERT_TRUE(is_directory(workset_directory));
        auto database_list = core::filter_directory_entry(
            workset_directory,
            [](const std::filesystem::directory_entry& entry) {
                auto xx = entry.path().stem().string();
                return RE2::FullMatch(entry.path().stem().string(), "TraceDb \\d{4}-\\d{2}-\\d{2} \\d{2}h\\d{2}m\\d{2}s");
            });
        if (!database_list.empty()) {
            auto& database_path = *std::max_element(std::execution::par, database_list.begin(), database_list.end(),
                                                    core::last_write_time_comparator{});
            std::unique_ptr<leveldb::DB> database;
            {
                leveldb::DB* ptr = nullptr;
                auto status = leveldb::DB::Open(leveldb::Options{}, database_path.string(), &ptr);
                ASSERT_TRUE(status.ok());
                database.reset(ptr);
            }
            std::unique_ptr<leveldb::Iterator> iterator{ database->NewIterator(leveldb::ReadOptions{}) };
            for (iterator->SeekToFirst(); iterator->Valid(); iterator->Next()) {
                std::cout << fmt::format("{}\n[event]{}\n", iterator->key().ToString(), iterator->value().ToString());
            }
        }
    }
}
