#include "pch.h"
#include <range/v3/action/transform.hpp>
#include <range/v3/view/unique.hpp>
#include <range/v3/algorithm/for_each_n.hpp>
#include <range/v3/range_for.hpp>
#include <range/v3/view/for_each.hpp>
#include <range/v3/view/iota.hpp>

namespace ranges::test
{
    TEST(Range, Take) {
        std::vector<int> v{ 1, 2, 3, 4, 5 };
        v | view::take_exactly(3) | action::transform([](int i) {
            EXPECT_LT(i, 4);
            return 0;
        });
    }

    TEST(Range, ForEachN) {
        std::vector<int> v{ 1, 2, 3, 4, 5 };
        ranges::for_each_n(v | view::unique, 3, [](int i) {
            EXPECT_LT(i, 4);
        });
    }
}
