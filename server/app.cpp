#include "stdafx.h"
#include "app.h"

namespace app
{
    void run() {
        const auto logger = core::console_logger_access("app");
        logger().info("application run");
        try {
            app::server{}.establish_sessions(
                core::make_pool_executor(std::thread::hardware_concurrency(),
                                         "ServerWorker"));
        } catch (...) {
            logger().error("catch exception \n{}", boost::current_exception_diagnostic_information());
        }
        logger().info("application quit");
    }
}
