#include "pch.h"

namespace std::test
{
    TEST(FileSystem, DefaultConstruct) {
        const std::filesystem::path p1;
        EXPECT_TRUE(std::empty(p1));
        EXPECT_TRUE(std::empty(std::filesystem::path{}));
        const std::filesystem::path p2{ "" };
        EXPECT_TRUE(std::empty(p2));
        const std::filesystem::path p3{ "F:/" };
        EXPECT_FALSE(std::empty(p3));
    }

    TEST(FileSystem, CurrentPath) {
        auto p = std::filesystem::current_path();
        EXPECT_TRUE(std::filesystem::is_directory(p));
        EXPECT_STREQ(p.generic_string().data(), "D:/Project/gallery360/test/gallery-test");
        auto fp = std::filesystem::current_path() / "test.cpp";
        EXPECT_STREQ(fp.generic_string().data(), "D:/Project/gallery360/test/gallery-test/test.cpp");
        EXPECT_TRUE(std::filesystem::is_regular_file(fp));
        std::ifstream fin{ fp, std::ios::binary };
        EXPECT_TRUE(fin.is_open());
        std::string str{ std::istreambuf_iterator<char>{ fin }, std::istreambuf_iterator<char>{} };
        EXPECT_EQ(std::size(str), std::filesystem::file_size(fp));
    }

    TEST(Exception, ThrowNonException) {
        std::string* ptr = nullptr;
        try {
            auto sp = std::make_shared<std::string>("123");
            ptr = sp.get();
            throw std::move(sp);
        } catch (std::shared_ptr<std::string> sp) {
            // CopyConstructable & Destructible
            EXPECT_EQ(ptr, sp.get());
        }
    }

    TEST(String, Replace) {
        auto replace = [](std::string& s) {
            auto suffix = "123";
            if (s.empty()) {
                return s.assign(fmt::format("/{}", suffix));
            }
            assert(s.front() == '/');
            return s.replace(s.rfind('/') + 1, s.size(), suffix);
        };
        std::string x = "/dash/init.mp4";
        EXPECT_STREQ(replace(x).c_str(), "/dash/123");
        x = "/init.mp4";
        EXPECT_STREQ(replace(x).c_str(), "/123");
        x.clear();
        EXPECT_STREQ(replace(x).c_str(), "/123");
    }

    TEST(String, Insert) {
        std::string x = "";
        x.insert(x.begin(), '/');
        EXPECT_STREQ(x.c_str(), "/");
        x = "123";
        x.insert(x.begin(), '/');
        EXPECT_STREQ(x.c_str(), "/123");
    }

    TEST(Variant, GetIf) {
        std::variant<std::monostate, int> v;
        EXPECT_FALSE(v.valueless_by_exception());
        EXPECT_EQ(v.index(), 0);
        EXPECT_TRUE(std::get_if<std::monostate>(&v) != nullptr);
        EXPECT_TRUE(std::get_if<int>(&v) == nullptr);
        EXPECT_TRUE(&std::get<std::monostate>(v) != nullptr);
        EXPECT_TRUE(&std::get<std::monostate>(v) == std::get_if<std::monostate>(&v));
    }

    TEST(Variant, Get) {
        std::variant<std::monostate, int> v;
        auto i = 0;
        EXPECT_THROW(i = std::get<int>(v), std::bad_variant_access);
    }
}
