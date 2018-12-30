#if _WIN32 && __cplusplus < 201703L
#define __cplusplus 201703L
#endif

#include "core/pch.h"
#include "core/meta.hpp"
#include "core/detail.hpp"
#include "core/type_trait.hpp"
#include "core/function_trait.hpp"
#include "core/member_function_trait.hpp"
#include "core/core.hpp"
#include "core/exception.hpp"
#include "core/guard.hpp"
#include "core/verify.hpp"
#include "core/core.cpp"

#include "network/pch.h"
#include "network/net.hpp"
#include "network/session_base.h"
#include "network/server.hpp"
#include "network/acceptor.hpp"
#include "network/net.cpp"
#include "network/acceptor.cpp"
#include "network/server.cpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <boost/program_options.hpp>

int main() {
    std::string ttt{ _NET_CONFIG_DIR };
    auto aaa = fmt::format("{}123", "abc");
    folly::MPMCQueue<int> q(2);
    q.writeIfNotFull(22);
    std::string s;
    std::cout << s << std::endl;
    std::filesystem::path project = std::filesystem::current_path().parent_path().parent_path().parent_path();
    auto x1 = core::filter_directory_entry(project, [](const std::filesystem::directory_entry& entry) {
        return entry.path().extension() == ".cpp";
    });
    auto x2 = core::tidy_directory_path(x1[0]);
    auto exec = core::make_pool_executor(7, "test");

    auto yyy = project.string();
    printf("hello from server_linux!\n");
    exec = nullptr;
    return 0;
}
