#include "pch.h"
#include "re2/re2.h"

namespace re2_test
{
    TEST(Re2, Submatch) {
        int i;
        std::string s;
        EXPECT_TRUE(RE2::FullMatch("ruby:1234", "(\\w+):(\\d+)", &s, &i));
        EXPECT_THAT(s, StrEq("ruby"));
        EXPECT_EQ(i, 1234);
        EXPECT_FALSE(RE2::FullMatch("ruby", "(.+)", &i));
        EXPECT_TRUE(RE2::FullMatch("ruby:1234", "(\\w+):(\\d+)", &s));
        i = 0;
        EXPECT_TRUE(RE2::FullMatch("ruby:1234", "(\\w+):(\\d+)", (void*)NULL, &i));
        EXPECT_EQ(i, 1234);
        EXPECT_FALSE(RE2::FullMatch("ruby:123456789123", "(\\w+):(\\d+)", &s, &i)); // integer overflow
        EXPECT_TRUE(RE2::FullMatch("TraceDb 2018-12-22 22h29m44s", "TraceDb \\d{4}-\\d{2}-\\d{2} \\d{2}h\\d{2}m\\d{2}s"));
    }

    TEST(Re2, PreCompiled) {
        int i;
        std::string s;
        RE2 re("(\\w+):(\\d+)");
        EXPECT_TRUE(re.ok()); // compiled; if not, see re.error();
        EXPECT_TRUE(RE2::FullMatch("ruby:1234", re, &s, &i));
        EXPECT_TRUE(RE2::FullMatch("ruby:1234", re, &s));
        EXPECT_TRUE(RE2::FullMatch("ruby:1234", re, (void*)NULL, &i));
        EXPECT_FALSE(RE2::FullMatch("ruby:123456789123", re, &s, &i));
    }

    TEST(Re2, Option) {
        RE2 re("(ab", RE2::Quiet); // don't write to stderr for parser failure
        EXPECT_FALSE(re.ok());     // can check re.error() for details
    }

    TEST(Re2, Replace) {
        {
            auto str = "tile9-576p-1500kbps_dash$Number$.m4s"s;
            EXPECT_TRUE(RE2::Replace(&str, "\\$Number\\$", folly::to<std::string>(666)));
            EXPECT_THAT(str, StrEq("tile9-576p-1500kbps_dash666.m4s"));
        }
        {
            auto str = "tile9-576p-1500kbps_dash$Number$$Number$.m4s"s;
            EXPECT_TRUE(RE2::Replace(&str, "\\$Number\\$", folly::to<std::string>(666)));
            EXPECT_THAT(str, StrEq("tile9-576p-1500kbps_dash666$Number$.m4s"));
            EXPECT_TRUE(RE2::Replace(&str, "\\$Number\\$", folly::to<std::string>(666)));
            EXPECT_THAT(str, StrEq("tile9-576p-1500kbps_dash666666.m4s"));
            EXPECT_FALSE(RE2::Replace(&str, "\\$Number\\$", folly::to<std::string>(666)));
        }
        {
            auto str = "banana"s;
            EXPECT_TRUE(RE2::GlobalReplace(&str, "ana", folly::to<std::string>(1)));
            EXPECT_THAT(str, StrEq("b1na"));
        }
        {
            auto str = "banana"s;
            EXPECT_TRUE(RE2::GlobalReplace(&str, "an", folly::to<std::string>(1)));
            EXPECT_THAT(str, StrEq("b11a"));
        }
    }

    TEST(Re2, Extract) {
        auto str = "tile9-576p-1500kbps_dash$Number$$Number$.m4s"s;
        auto extract = "123"s;
        EXPECT_TRUE(RE2::Extract(str, "\\$Number\\$", folly::to<std::string>(666), &extract));
        EXPECT_THAT(str, StrEq("tile9-576p-1500kbps_dash$Number$$Number$.m4s"));
        EXPECT_THAT(extract, StrNe("123"));
        EXPECT_THAT(extract, StrEq("666"));
        EXPECT_TRUE(RE2::Extract(str, "\\$Number\\$", folly::to<std::string>(666), &extract));
        EXPECT_TRUE(RE2::Extract(str, "\\$Number\\$", folly::to<std::string>(666), &extract));
        EXPECT_THAT(extract, StrEq("666"));
    }

    TEST(Re2, QuoteMeta) {
        EXPECT_THAT(RE2::QuoteMeta(".-?$"), StrEq("\\.\\-\\?\\$"));
        EXPECT_THAT(RE2::QuoteMeta("123abc"), StrEq("123abc"));
    }

    TEST(Re2, Consume) {
        {
            std::string str{ "abcd12b456" };
            re2::StringPiece input{ str };
            std::string consume;
            auto index = 0;
            while (RE2::Consume(&input, "(\\w+?)b", &consume) && ++index) {
                if (index == 1) {
                    EXPECT_THAT(input.data(), StrEq("cd12b456"));
                    EXPECT_THAT(consume, StrEq("a"));
                } else if (index == 2) {
                    EXPECT_THAT(input.data(), StrEq("456"));
                    EXPECT_THAT(consume, StrEq("cd12"));
                } else {
                    FAIL();
                }
            }
        }
        {
            RE2 r{ "\\s*(\\w+)" }; // matches a word, possibly proceeded by whitespace
            std::string word;
            std::string s{ "   aaa b!@#$@#$cccc" };
            re2::StringPiece input(s);
            ASSERT_TRUE(RE2::Consume(&input, r, &word));
            ASSERT_EQ(word, "aaa") << " input: " << input;
            ASSERT_TRUE(RE2::Consume(&input, r, &word));
            ASSERT_EQ(word, "b") << " input: " << input;
            ASSERT_FALSE(RE2::Consume(&input, r, &word)) << " input: " << input;
        }
    }

    TEST(Re2, FindAndConsume) {
        std::string str{ "abcd12b456" };
        re2::StringPiece input{ str };
        std::string consume;
        auto index = 0;
        while (RE2::FindAndConsume(&input, "(.b)", &consume) && ++index) {
            if (index == 1) {
                EXPECT_THAT(input.data(), StrEq("cd12b456"));
                EXPECT_THAT(consume, "ab");
            } else if (index == 2) {
                EXPECT_THAT(input.data(), StrEq("456"));
                EXPECT_THAT(consume, "2b");
            } else {
                FAIL();
            }
        }
    }
}
