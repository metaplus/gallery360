#include "pch.h"
#include <fmt/format.h>
#include <folly/futures/Future.h>
#include <folly/stop_watch.h>
#include "network/component.h"

using net::component::dash_manager;

namespace net_test
{
    TEST(DashManager, ParseMpdConfig) {
        core::set_cpu_executor(3);
        {
            auto manager = dash_manager::create_parsed("http://localhost:8900/dash/tos_srd_4K.mpd").get();
            auto spatial_size = manager.scale_size();
            auto grid_size = manager.grid_size();
            EXPECT_EQ(grid_size, std::make_pair(3, 3));
            EXPECT_EQ(spatial_size, std::make_pair(3840, 1728));
        }
        {
            auto manager = dash_manager::create_parsed("http://localhost:8900/dash/NewYork/5k/NewYork_5k.mpd").get();
            auto spatial_size = manager.scale_size();
            auto grid_size = manager.grid_size();
            EXPECT_EQ(grid_size, std::make_pair(3, 3));
            EXPECT_EQ(spatial_size, std::make_pair(3840, 1920));
        }
    }

    TEST(DashManager, PathRegex) {
        auto path = "tile9-576p-1500kbps_dash$Number$.m4s"s;
        auto path_regex = [](std::string& path, auto index) {
            static const std::regex pattern{ "\\$Number\\$" };
            return std::regex_replace(path, pattern, fmt::to_string(index));
        };
        EXPECT_EQ(path_regex(path, 1), "tile9-576p-1500kbps_dash1.m4s");
        EXPECT_EQ(path_regex(path, 10), "tile9-576p-1500kbps_dash10.m4s");
    }
}
