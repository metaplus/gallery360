#include "pch.h"
#include <folly/executors/CPUThreadPoolExecutor.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <absl/numeric/int128_no_intrinsic.inc>
#include <range/v3/view/iota.hpp>

namespace core::test
{
    TEST(File, PathOfDirectory) {
        auto p = core::file_path_of_directory("C:/AppData", ".exe");
        EXPECT_EQ(p.generic_string(), "C:/AppData/AppData.exe");
    }

#pragma warning(disable:244 267 101)
    TEST(Overload, Base) {
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

    TEST(Logger, ConsoleLoggerAccess) {
        auto name = "TestLogger"s;
        {
            auto process = ""s;
            auto logger_access = core::console_logger_access(name, [&](auto&) {
                process.assign(name);
            });
            EXPECT_TRUE(process.empty());
            auto& logger = logger_access();
            EXPECT_EQ(name, logger.name());
            EXPECT_EQ(process, name);
        }
        {
            auto logger_access = core::console_logger_access(name);
            EXPECT_ANY_THROW(logger_access());
        }
    }

    TEST(Time, TimeFormat) {
        XLOG(INFO) << core::time_format();
        XLOG(INFO) << core::time_format("%Y%m%d.%H%M%S");
        XLOG(INFO) << core::local_date_time();
    }

    TEST(Executor, JoinThreadPoolExecutor) {
        auto executor = core::make_pool_executor(2, "TestPool1");
        auto repeat_dispatch = [&executor](int n) {
            while (n--) {
                executor->add([] {
                    std::this_thread::sleep_for(200ms);
                });
            }
        };
        folly::stop_watch<milliseconds> watch;
        {
            repeat_dispatch(2);
            executor->join();
        }
        EXPECT_GE(watch.lap(), 200ms);
        executor = core::make_pool_executor(2, "TestPool");
        watch.reset();
        {
            repeat_dispatch(3);
            executor->join();
        }
        EXPECT_GE(watch.lap(), 400ms);
    }

    TEST(Executor, DuplicateSetCpuExecutor) {
        {
            auto executor = core::make_pool_executor(2, "TestPool");
            EXPECT_EQ(executor->numThreads(), 2);
            EXPECT_EQ(executor.use_count(), 1);
        }
        {
            auto executor = core::make_pool_executor(2, "TestPool");
            EXPECT_EQ(executor->numThreads(), 2);
            EXPECT_EQ(executor.use_count(), 1);
        }
        {
            auto executor = core::set_cpu_executor(2, "TestPool");
            EXPECT_EQ(executor->numThreads(), 2);
            EXPECT_EQ(executor.use_count(), 2);
        }
        {
            auto cpu_executor = folly::getCPUExecutor();
            EXPECT_EQ(cpu_executor.use_count(), 2);
            EXPECT_EQ(dynamic_cast<folly::CPUThreadPoolExecutor&>(*cpu_executor).numThreads(), 2);
            dynamic_cast<folly::CPUThreadPoolExecutor&>(*cpu_executor).join();
            EXPECT_EQ(dynamic_cast<folly::CPUThreadPoolExecutor&>(*cpu_executor).numThreads(), 0);
        }
        {
            auto executor = core::make_pool_executor(5, "TestPool");
            EXPECT_EQ(executor->numThreads(), 5);
            EXPECT_EQ(executor.use_count(), 1);
            folly::setCPUExecutor(executor);
            EXPECT_EQ(dynamic_cast<folly::CPUThreadPoolExecutor&>(*folly::getCPUExecutor()).numThreads(), 5);
        }
    }

    TEST(Logger, AsyncCreate) {
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("F:/GWorkSet/test.sink.log", true);
        auto logger1 = core::make_async_logger("test-1", sink);
        auto logger2 = core::make_async_logger("test-2", sink);
        auto workload = [](auto logger) {
            return [&] {
                for (auto i : ranges::view::ints(0, 500)) {
                    logger->info("{}", i);
                }
            };
        };
        std::thread th1{ workload(logger1) };
        std::thread th2{ workload(logger2) };
        th1.join();
        th2.join();
        spdlog::drop_all();
    }
}
