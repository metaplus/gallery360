#include "stdafx.h"
#include "CppUnitTest.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
namespace test
{
    TEST_CLASS(SpdlogTest) {
public:

    TEST_METHOD(StdOutExample) {
        // create color multi threaded logger
        auto console = spdlog::stdout_color_mt("console");
        console->info("Welcome to spdlog!");
        console->error("Some error message with arg: {}", 1);

        auto err_logger = spdlog::stderr_color_mt("stderr");
        err_logger->error("Some error message");
        
        // Formatting examples
        console->warn("Easy padding in numbers like {:08d}", 12);
        console->critical("Support for int: {0:d};  hex: {0:x};  oct: {0:o}; bin: {0:b}", 42);
        console->info("Support for floats {:03.2f}", 1.23456);
        console->info("Positional args are {1} {0}..", "too", "supported");
        console->info("{:<30}", "left aligned");

        spdlog::get("console")->info("loggers can be retrieved from a global registry using the spdlog::get(logger_name)");

        // Runtime log levels
        spdlog::set_level(spdlog::level::info); // Set global log level to info
        console->debug("This message should not be displayed!");
        console->set_level(spdlog::level::trace); // Set specific logger's log level
        console->debug("This message should be displayed..");

        // Customize msg format for all loggers
        spdlog::set_pattern("[%H:%M:%S %z] [%n] [%^---%L---%$] [thread %t] %v");
        console->info("This an info message with custom format");

        // Compile time log levels
        // define SPDLOG_DEBUG_ON or SPDLOG_TRACE_ON
        SPDLOG_TRACE(console, "Enabled only #ifdef SPDLOG_TRACE_ON..{} ,{}", 1, 3.23);
        SPDLOG_DEBUG(console, "Enabled only #ifdef SPDLOG_DEBUG_ON.. {} ,{}", 1, 3.23);
    }

    TEST_METHOD(MultiSinkExample)
    {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::warn);
        console_sink->set_pattern("[multi_sink_example] [%^%l%$] %v");

        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/multisink.txt", true);
        file_sink->set_level(spdlog::level::trace);

        spdlog::logger logger("multi_sink", { console_sink, file_sink });
        logger.set_level(spdlog::level::debug);
        logger.warn("this should appear in both console and file");
        logger.info("this message should not appear in the console, only in the file");
    }
    };
}