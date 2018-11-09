#include "pch.h"

namespace std
{
    TEST(FileSystem,DefaultConstruct) {
        const std::filesystem::path p1;
        EXPECT_TRUE(std::empty(p1));
        EXPECT_TRUE(std::empty(std::filesystem::path{}));
        const std::filesystem::path p2{ "" };
        EXPECT_TRUE(std::empty(p2));
        const std::filesystem::path p3{ "F:/" };
        EXPECT_FALSE(std::empty(p3));
    }
}
