#include "stdafx.h"
#include "app.h"

namespace app
{
    void run() {
        auto logger = spdlog::stdout_color_mt("app");
        logger->info("application run");
        try {
            const auto executor = core::set_cpu_executor(2, "ServerPool");
            app::server{}.establish_sessions(executor);
        } catch (...) {
            logger->error("catch exception \n{}", boost::current_exception_diagnostic_information());
        }
        logger->info("application quit");
    }
}
