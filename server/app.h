#pragma once
#include <variant>

namespace app
{
    void parse_options(int argc, char* argv[]);
    void run();

    namespace config
    {
        enum element
        {
            bandwidth_download_rate,
            bandwidth_upload_rate,
            bandwidth_limit,
            bandwidth_limit_period_span,
            bandwidth_limit_period_offset,
            last,
        };

        std::variant<bool, int, std::string> load_element(element type);
        bool enable_element(element type, bool enable);
    }
}
