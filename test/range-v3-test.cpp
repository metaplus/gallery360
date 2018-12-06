#include "pch.h"

namespace range_test
{
    TEST(Ints, Base) {
        auto i = ranges::view::ints(1, 5);
        EXPECT_EQ(ranges::size(i), 4);
    }
}
