#include "pch.h"

namespace core
{
    TEST(Core,FileOfDirectory) {
        auto p = file_path_of_directory("C:/AppData", ".exe");
        EXPECT_THAT(p.generic_string(), StrEq("C:/AppData/AppData.exe"));
    }
}
