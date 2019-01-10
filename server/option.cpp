#include "stdafx.h"
#include "app.h"
#include <boost/container/flat_map.hpp>

auto logger = core::console_logger_access("app.options");

auto logged_wrapper = [](auto info, auto notifier) {
    return [=](auto option) {
        logger().info("parse {} value {}", info, option);
        notifier(option);
    };
};

struct option_detail final
{
    std::string description;
    boost::program_options::value_semantic* semantic = nullptr;
};

const boost::container::flat_map<std::string, option_detail> option_details{
    {
        "help,h", {
            "help screen"
        }
    },
    {
        "config,c", {
            "directory of config file",
            boost::program_options::value<std::string>()->notifier(
                logged_wrapper(
                    "config directory",
                    [](std::string config) {
                        net::add_config_path(config);
                    }))
        }
    }
};

auto iterate_option_details = [](auto process) {
    for (auto& [option, detail] : option_details) {
        process(option, detail);
    }
};

namespace app
{
    void parse_options(int argc, char* argv[]) {
        logger().info("parsing {} launching parameters", argc - 1);
        boost::program_options::options_description description{ "server launch options" };
        iterate_option_details(
            [add_options = description.add_options()](auto& option, auto& detail) mutable {
                detail.semantic
                    ? add_options(option.data(), detail.semantic, detail.description.data())
                    : add_options(option.data(), detail.description.data());
            });
        boost::program_options::variables_map options;
        boost::program_options::store(
            boost::program_options::parse_command_line(argc, argv, description),
            options);
        boost::program_options::notify(options);
        if (options.count("help")) {
            fmt::print("{}", description);
            std::exit(EXIT_SUCCESS);
        }
    }
}
