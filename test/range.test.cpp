#include "pch.h"
#include <range/v3/view/bounded.hpp>
#include <range/v3/view/cartesian_product.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/counted.hpp>
#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/exclusive_scan.hpp>
#include <range/v3/view/for_each.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/partial_sum.hpp>
#include <range/v3/view/take.hpp>
#include <range/v3/view/take_while.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/unique.hpp>
#include <range/v3/view/view.hpp>
#include <range/v3/action/transform.hpp>
#include <range/v3/action/take_while.hpp>
#include <range/v3/algorithm/for_each_n.hpp>

namespace ranges::test
{
    TEST(View, Take) {
        std::vector<int> v{ 1, 2, 3, 4, 5 };
        v | view::take_exactly(3) | action::transform([](int i) {
            EXPECT_LT(i, 4);
            return 0;
        });
    }

    TEST(Algorithm, ForEachN) {
        std::vector<int> v{ 1, 2, 3, 4, 5 };
        for_each_n(v | view::unique, 3, [](int i) {
            EXPECT_LT(i, 4);
        });
    }

    TEST(View, PartialSum) {
        auto rg = { 1, 2, 3 };
        auto rg2 = rg | view::partial_sum(std::plus<int>{});
        for (auto&& [index, value] : rg2 | view::enumerate) {
            if (index == 0)
                EXPECT_EQ(value, 1);
            else if (index == 1)
                EXPECT_EQ(value, 3);
            else if (index == 2)
                EXPECT_EQ(value, 6);
            else
                EXPECT_TRUE(false);
        }
    }

    TEST(View, ExclusiveScan) {
        auto rg = { 1, 2, 3 };
        auto rg2 = rg | view::exclusive_scan(1, std::plus<int>{});
        EXPECT_EQ(3, size(rg2));
        for (auto&& [index, value] : rg2 | view::enumerate) {
            if (index == 0)
                EXPECT_EQ(value, 1);
            else if (index == 1)
                EXPECT_EQ(value, 2);
            else if (index == 2)
                EXPECT_EQ(value, 4);
            else
                EXPECT_TRUE(false);
        }
        auto rg3 = rg | view::exclusive_scan("ab"s, [](std::string&& str,int i) {
            return str + std::to_string(i);
        });
        EXPECT_EQ(3, size(rg3));
        for (auto&& [index, str] : rg3 | view::enumerate) {
            if (index == 0)
                EXPECT_EQ(str, "ab");
            else if (index == 1)
                EXPECT_EQ(str, "ab1");
            else if (index == 2)
                EXPECT_EQ(str, "ab12");
            else
                EXPECT_TRUE(false);
        }
    }

    TEST(View, CartesianProduct) {
        auto rg = { 1, 2, 3 };
        std::vector<std::string> rg2 = view::cartesian_product(rg, rg)
            | view::transform([](std::tuple<int, int> tp) {
                auto& [a, b] = tp;
                auto str = fmt::format("{}x{}", a, b);
                XLOG(INFO) << str;
                return str;
            })
            | view::take_while([](const std::string& str) {
                EXPECT_FALSE(str.empty());
                return true;
            })
            | view::take(20);
        EXPECT_EQ(9, size(rg2));
    }
}
