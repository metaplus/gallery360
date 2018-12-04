#include "pch.h"
#include <leveldb/db.h>

namespace leveldb_test
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
                EXPECT_NE(database, nullptr);
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
}
