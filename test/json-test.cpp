#include "pch.h"
#include <nlohmann/json.hpp>

namespace json_test
{
    using namespace nlohmann;
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
}
