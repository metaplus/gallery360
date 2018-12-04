#include "pch.h"

namespace core_test
{
    TEST(File, PathOfDirectory) {
        auto p = core::file_path_of_directory("C:/AppData", ".exe");
        EXPECT_THAT(p.generic_string(), StrEq("C:/AppData/AppData.exe"));
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
            EXPECT_THAT(process, IsEmpty());
            auto& logger = logger_access();
            EXPECT_EQ(name, logger->name());
            EXPECT_EQ(process, name);
        }
        {
            auto logger_access = core::console_logger_access(name);
            EXPECT_ANY_THROW(logger_access());
        }
    }

    TEST(Time, TimeFormat) {
        XLOG(INFO) << core::time_format();
        XLOG(INFO) << core::date_format();
    }

    TEST(Executor, JoinThreadPoolExecutor) {
        auto executor = core::make_pool_executor(2, "TestPool1");
        folly::stop_watch<milliseconds> watch;
        {
            executor->add([] {
                std::this_thread::sleep_for(200ms);
            });
            executor->add([] {
                std::this_thread::sleep_for(200ms);
            });
            executor->join();
        }
        EXPECT_GE(watch.lap(), 200ms);
        executor = core::make_pool_executor(2, "TestPool");
        watch.lap();
        {
            executor->add([] {
                std::this_thread::sleep_for(200ms);
            });
            executor->add([] {
                std::this_thread::sleep_for(200ms);
            });
            executor->add([] {
                std::this_thread::sleep_for(200ms);
            });
            executor->join();
        }
        EXPECT_GE(watch.lap(), 400ms);
    }
}
