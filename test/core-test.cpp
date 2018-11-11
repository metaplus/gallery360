#include "pch.h"

namespace core_test
{
    TEST(Core, FileOfDirectory) {
        auto p = core::file_path_of_directory("C:/AppData", ".exe");
        EXPECT_THAT(p.generic_string(), StrEq("C:/AppData/AppData.exe"));
    }

    #pragma warning(disable:244 267 101)
    TEST(Core, Overload) {
        using variant = std::variant<int, long, double, std::string>;
        variant v{ 1 };
        core::overload overload{
            [](auto arg) -> int {
                return arg * 2;
            },
            [](double arg) -> int {
                return arg + 0.1;
            },
            [](const std::string& arg) -> int {
                return arg.size();
            }
        };
        auto r1 = std::visit(overload, variant{ 1 });
        EXPECT_EQ(r1, 2);
        auto r2 = std::visit(overload, variant{ 1.2 });
        EXPECT_EQ(r2, 1);
        auto r3 = std::visit(overload, variant{ std::string{ "123" } });
        EXPECT_EQ(r3, std::string{ "123" }.size());
    }
}
