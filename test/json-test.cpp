#include "pch.h"
#include <nlohmann/json.hpp>
#include <folly/logging/xlog.h>

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
}
