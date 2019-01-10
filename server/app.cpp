#include "stdafx.h"
#include "app.h"

namespace app
{
    void run() {
        const auto logger = core::console_logger_access("app");
        logger().info("application run");
        try {
            const auto executor = core::set_cpu_executor(std::thread::hardware_concurrency(), "ServerWorker");
            app::server{}.establish_sessions(executor);
        } catch (...) {
            logger().error("catch exception \n{}", boost::current_exception_diagnostic_information());
        }
        logger().info("application quit");
    }
}
