#include "pch.h"

namespace std_test
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

    TEST(Exception, ThrowNonException) {
        std::string* ptr = nullptr;
        try {
            auto sp = std::make_shared<std::string>("123");
            ptr = sp.get();
            throw std::move(sp);
        }
        catch (std::shared_ptr<std::string> sp) { // CopyConstructable & Destructible
            EXPECT_EQ(ptr, sp.get());
        }
    }
}
