#include "pch.h"
#include <date/date.h>
#include <date/tz.h>

namespace date_test
{
    TEST(Date, Base) {
        using namespace std::chrono;
        EXPECT_ANY_THROW(date::current_zone());
        XLOG(INFO) << date::format("%Y-%m-%d %H:%M:%S", system_clock::now());
    }
}
