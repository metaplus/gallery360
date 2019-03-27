#include "stdafx.h"
#include "app.h"
#include <folly/Lazy.h>
#include <boost/container/flat_map.hpp>
#include <boost/logic/tribool.hpp>
#include <range/v3/view/iota.hpp>

namespace app
{
    void run() {
        const auto logger = core::console_logger_access("app");
        logger().info("application run");
        try {
            app::server{}
                .parse_bandwidth_limit()
                .establish_sessions(core::make_pool_executor(
                    std::thread::hardware_concurrency(), "ServerWorker"));
        } catch (...) {
            logger().error("catch exception \n{}", boost::current_exception_diagnostic_information());
        }
        logger().info("application quit");
    }

    auto element_manual_enabler = folly::lazy([] {
        return boost::container::flat_map<
            config::element, std::pair<boost::logic::tribool, std::string>>{
            { config::bandwidth_limit, { boost::indeterminate, "Net.Bandwidth.Fluctuate" } },
            { config::bandwidth_limit_period_span, { boost::indeterminate, "Net.Bandwidth.Period.Span" } },
            { config::bandwidth_limit_period_offset, { boost::indeterminate, "Net.Bandwidth.Period.Offset" } },
            { config::bandwidth_download_rate_max, { boost::indeterminate, "Net.Bandwidth.Limit.Download.Max" } },
            { config::bandwidth_download_rate_min, { boost::indeterminate, "Net.Bandwidth.Limit.Download.Min" } },
            { config::bandwidth_upload_rate_max, { boost::indeterminate, "Net.Bandwidth.Limit.Upload.Max" } },
            { config::bandwidth_upload_rate_min, { boost::indeterminate, "Net.Bandwidth.Limit.Upload.Min" } },
        };
    });

    std::variant<bool, int, std::string>
    config::load_element(element type) {
        const auto& [enabled, entry_name] = element_manual_enabler().at(type);
        switch (type) {
        case bandwidth_limit:
            if (enabled) {
                return net::config_entry<bool>(entry_name);
            }
            return false;
        default:
            if (!enabled) return false;
            return net::config_entry<int>(entry_name);
        }
    }

    bool config::enable_element(element type, bool enable) {
        const auto enabler_exist = element_manual_enabler().count(type);
        element_manual_enabler().at(type).first = enable;
        return enabler_exist;
    }
}
