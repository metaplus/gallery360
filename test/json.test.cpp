#include "pch.h"
#include <nlohmann/json.hpp>

namespace nlohmann::test
{
    TEST(Base, OperatorAccess) {
        json j;
        j["pi"] = 3.141;
        j["happy"] = true;
        j["name"] = "Niels";
        j["nothing"] = nullptr;
        j["answer"]["everything"] = 42;
        j["list"] = { 1, 0, 2 };
        j["object"] = { { "currency", "USD" }, { "value", 42.99 } };
        XLOG(INFO) << j.dump();
    }

    TEST(Generate, Server) {
        json j;
        j["net"]["protocal"] = "http";
        j["net"]["server"]["port"] = 8900;
        j["net"]["server"]["directories"]["root"] = "D:/Media";
        j["net"]["server"]["directories"]["log"] = nullptr;
        auto str = j.dump(0);
        XLOG(INFO) << j.dump();
    }

    enum TaskState
    {
        TS_STOPPED,
        TS_RUNNING,
        TS_COMPLETED,
        TS_INVALID = -1,
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(TaskState, {
        {TS_INVALID, nullptr},
        {TS_STOPPED, "stopped"},
        {TS_RUNNING, "running"},
        {TS_COMPLETED, "completed"},
        }
    );

    TEST(Enum, Base) {
        json j = TS_STOPPED;
        EXPECT_TRUE(j == "stopped");
        json j3 = "running";
        EXPECT_TRUE(j3.get<TaskState>() == TS_RUNNING);
        json jPi = 3.14;
        EXPECT_TRUE(jPi.get<TaskState>() == TS_INVALID);
    }

    TEST(Array, PushBack) {
        json ar;
        ar.push_back("1");
        ar.push_back(true);
        ar.push_back(TS_RUNNING);
        XLOG(INFO) << ar.dump();
    }

    TEST(Base, GetTo) {
        auto j = "{ \"happy\": true, \"pi\": 3.141 }"_json;
        {
            auto h = false;
            EXPECT_TRUE(j["happy"].get_to(h));
            EXPECT_TRUE(h);
        }
        {
            auto h = false;
            EXPECT_NO_THROW(j["happy2"]);
            EXPECT_THROW(j["happy2"].get_to(h), nlohmann::detail::type_error);
            EXPECT_TRUE(j.value("happy", false));
            EXPECT_NO_THROW(j.value("happy2", nullptr));
            auto j2 = "{ \"happy\": true, \"pi\": 3.141 }"_json;
            EXPECT_EQ(j2.value("happy2", 1), 1);
        }
        {
            auto p = 0.;
            EXPECT_EQ(j["pi"].get_to(p), 3.141);
            EXPECT_EQ(p, 3.141);
        }
    }
}
