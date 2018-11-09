#include "pch.h"

namespace folly
{
    TEST(Future, Exchange) {
        auto f = folly::makeFuture(1);
        auto i = std::exchange(f, folly::makeFuture(2)).value();
        EXPECT_EQ(i, 1);
        auto i2 = std::exchange(f, folly::makeFuture(3)).get();
        EXPECT_EQ(i2, 2);
        EXPECT_EQ(f.value(), 3);
    }
}