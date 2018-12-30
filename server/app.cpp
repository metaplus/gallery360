#include "stdafx.h"
#include "app.h"

namespace app
{
    void run() {
        auto logger = spdlog::stdout_color_mt("app");
        logger->info("application run");
        try {
            server{}.spawn_session_builder()
                    .loop_listen();
        } catch (...) {
            logger->error("catch exception \n{}", boost::current_exception_diagnostic_information());
        }
        logger->info("application quit");
    }
}
