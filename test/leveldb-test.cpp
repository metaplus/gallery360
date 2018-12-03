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
}
