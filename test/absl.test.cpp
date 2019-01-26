#include "pch.h"
#include <absl/strings/str_cat.h>
#include <absl/strings/str_format.h>
#include <absl/strings/str_join.h>
#include <absl/strings/str_replace.h>
#include <absl/strings/str_split.h>
#include <absl/time/civil_time.h>
#include <absl/time/clock.h>
#include <absl/time/time.h>

namespace absl::test
{
    TEST(Time, TimeZone) {
        TimeZone z;
        EXPECT_FALSE(LoadTimeZone("America/New_York", &z));
        EXPECT_FALSE(LoadTimeZone("Asia/Shanghai", &z));
        auto z2 = FixedTimeZone(8 * 60 * 60);
        XLOG(INFO) << FormatTime(Now(), z2);
        XLOG(INFO) << FormatTime("%Y%m%d.%H%M%S", Now(), z2);
    }

    TEST(String, Cat) {
        XLOG(INFO) << StrCat("trace.", FormatTime("%Y%m%d.%H%M%S", Now(), FixedTimeZone(8 * 60 * 60)));
    }

    TEST(String, Join) {
        std::string s1 = "111";
        std::string_view s2 = "222";
        auto* s3 = "333";
        std::initializer_list<const char*> in = { s1.data(), s2.data(), s3 };
        {
            auto s = absl::StrJoin(in, "-");
            EXPECT_EQ(s, "111-222-333");
        }
        {
            auto s = absl::StrJoin({ "1"sv, "3"sv }, "2");
            EXPECT_EQ(s, "123");
        }
    }
}
