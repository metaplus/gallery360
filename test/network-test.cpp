#include "pch.h"
#include <fmt/format.h>
#include <folly/futures/Future.h>
#include <folly/stop_watch.h>
#include <re2/re2.h>
#include "network/component.h"
#include "network/pch.h"
#include "network/net.hpp"

using net::component::dash_manager;

namespace net_test
{
    TEST(DashManager, ParseMpdConfig) {
        core::set_cpu_executor(3);
        {
            auto manager = dash_manager::create_parsed("http://localhost:33666/Output/NewYork/5x3/NewYork.mpd").get();
            auto spatial_size = manager.frame_size();
            auto grid_size = manager.grid_size();
            EXPECT_EQ(grid_size, std::make_pair(5, 3));
            EXPECT_EQ(spatial_size, std::make_pair(3840, 1920));
        }
        {
            auto manager = dash_manager::create_parsed("http://localhost:8900/dash/tos_srd_4K.mpd").get();
            auto spatial_size = manager.frame_size();
            auto grid_size = manager.grid_size();
            EXPECT_EQ(grid_size, std::make_pair(3, 3));
            EXPECT_EQ(spatial_size, std::make_pair(3840, 1728));
        }
        {
            auto manager = dash_manager::create_parsed("http://localhost:8900/dash/NewYork/5k/NewYork_5k.mpd").get();
            auto spatial_size = manager.frame_size();
            auto grid_size = manager.grid_size();
            EXPECT_EQ(grid_size, std::make_pair(3, 3));
            EXPECT_EQ(spatial_size, std::make_pair(3840, 1920));
        }
        {
            auto manager = dash_manager::create_parsed("http://localhost:8900/Output/NewYork/NewYork.mpd").get();
            auto spatial_size = manager.frame_size();
            auto grid_size = manager.grid_size();
            EXPECT_EQ(grid_size, std::make_pair(3, 3));
            EXPECT_EQ(spatial_size, std::make_pair(3840, 1920));
        }
    }

    TEST(DashManager, PathRegex) {
        auto path = "tile9-576p-1500kbps_dash$Number$.m4s"s;
        auto path_regex = [](std::string& path, auto index) {
            static const RE2 pattern{ "\\$Number\\$" };
            return RE2::Replace(&path, pattern, fmt::to_string(index));
        };
        {
            auto p = path;
            EXPECT_TRUE(path_regex(p, 1));
            EXPECT_EQ(p, "tile9-576p-1500kbps_dash1.m4s");
        }
        {
            auto p = path;
            EXPECT_TRUE(path_regex(p, 10));
            EXPECT_EQ(p, "tile9-576p-1500kbps_dash10.m4s");
        }
    }

    TEST(Config, Json) {
        {
            auto port = net::config_json_entry({ "net", "server", "port" });
            EXPECT_EQ(33666, port.get<unsigned>());
        }
        {
            auto port = net::config_entry<unsigned>("net.server.port");
            auto dir = net::config_entry<std::string>("net.server.directories.root");
            EXPECT_EQ(33666, port);
            EXPECT_EQ(dir, "D:/Media");
        }
    }
}
