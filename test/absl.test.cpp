#include "pch.h"
#include <absl/strings/str_cat.h>
#include <absl/strings/str_format.h>
#include <absl/strings/str_join.h>
#include <absl/strings/str_replace.h>
#include <absl/strings/str_split.h>
#include <absl/time/civil_time.h>
#include <absl/time/clock.h>
#include <absl/time/time.h>

//#pragma comment(lib, "absl_based")
//#pragma comment(lib, "absl_typesd")
//#pragma comment(lib, "absl_stringsd")

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
        const std::string s = "123";
        const auto s2 = StrCat(s, "abc");
        EXPECT_EQ("123"s, s);
        EXPECT_EQ("123abc"s, s2);
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

    TEST(String, Split) {
        std::filesystem::path p{ R"(C:\GWorkset\Data\trace.20190202.011131\analyze\fov.csv)" };
        ASSERT_TRUE(is_regular_file(p));
        std::ifstream in{ p };
        ASSERT_TRUE(in.is_open());
        std::vector<std::string> labels;
        std::vector<std::map<std::string_view, std::string>> contents;
        for (std::string line; std::getline(in, line, '\n') && !line.empty();) {
            if (labels.empty()) {
                labels = absl::StrSplit(line, ',');
                EXPECT_EQ(labels.at(0), "id");
                EXPECT_EQ(labels.at(1), "time");
                EXPECT_EQ(labels.at(2), "y");
                EXPECT_EQ(labels.at(3), "x");
                EXPECT_EQ(labels.at(4), "col");
                EXPECT_EQ(labels.at(5), "row");
                continue;
            }
            auto label_value_pair = [
                    &labels, values = std::vector<std::string>{ absl::StrSplit(line, ',') }
                ](int index) mutable
            -> decltype(contents)::value_type::value_type {
                return { labels.at(index), std::move(values.at(index)) };
            };
            contents.emplace_back(
                decltype(contents)::value_type{
                    label_value_pair(0),
                    label_value_pair(1),
                    label_value_pair(2),
                    label_value_pair(3),
                    label_value_pair(4),
                    label_value_pair(5),
                }
            );
        }
        XLOGF(INFO, "csv vector size {} capacity {}", contents.size(), contents.capacity());
    }
}
