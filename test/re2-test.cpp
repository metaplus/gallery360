#include "pch.h"
#include "re2/re2.h"

namespace re2_test
{
    TEST(Re2, Submatch) {
        int i;
        std::string s;
        EXPECT_TRUE(RE2::FullMatch("ruby:1234", "(\\w+):(\\d+)", &s, &i));
        EXPECT_THAT(s ,StrEq("ruby"));
        EXPECT_EQ(i , 1234);
        EXPECT_FALSE(RE2::FullMatch("ruby", "(.+)", &i));
        EXPECT_TRUE(RE2::FullMatch("ruby:1234", "(\\w+):(\\d+)", &s));
        i = 0;
        EXPECT_TRUE(RE2::FullMatch("ruby:1234", "(\\w+):(\\d+)", (void*)NULL, &i));
        EXPECT_EQ(i, 1234);
        EXPECT_FALSE(RE2::FullMatch("ruby:123456789123", "(\\w+):(\\d+)", &s, &i)); // integer overflow
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

    TEST(Re2,FindAndConsume) {
        std::string str("abcd12b456");
        RE2 regex("(.b)");
        re2::StringPiece input(str);
        std::string consume;
        auto index = 0;
        while (RE2::FindAndConsume(&input, regex, &consume) && ++index) {
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
