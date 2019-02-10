#include "pch.h"
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace spdlog::test
{
    TEST(Logger, Async) {
        XLOG(INFO) << core::local_date_time("%Y-%m-%d %H:%M:%S.%E*f");
        {
            auto logger = basic_logger_mt<async_factory>("test", "F:/GWorkSet/test_log.txt");
            EXPECT_EQ(logger->sinks().size(), 1);
            logger->info("hello");
            logger->warn("world");
            drop_all();
        }
        std::this_thread::sleep_for(2s);
        XLOG(INFO) << core::local_date_time("%Y-%m-%d %H:%M:%S.%E*f");
        {
            auto logger = basic_logger_mt<async_factory>("test", "F:/GWorkSet/test_log.txt");
            auto logger2 = basic_logger_mt<async_factory>("test-2", "F:/GWorkSet/test_log.txt");
            logger->sinks().push_back(std::make_shared<sinks::basic_file_sink_mt>("F:/GWorkSet/test_log_2.txt"));
            EXPECT_EQ(logger->sinks().size(), 2);
            EXPECT_EQ(logger2->sinks().size(), 1);
            logger->info("hello-2");
            logger->warn("world-2");
            logger2->info("hello-2-2");
            logger2->warn("world-2-2");
            drop_all();
        }
    }
}
