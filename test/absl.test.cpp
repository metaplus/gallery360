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

    TEST(String, Append) {
        XLOG(INFO) << StrCat("trace.", FormatTime("%Y%m%d.%H%M%S", Now(), FixedTimeZone(8 * 60 * 60)));
    }

}
