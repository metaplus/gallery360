#if _WIN32 && __cplusplus < 201703L
#define __cplusplus 201703L
#endif

#include "core/pch.h"
#include "core/meta.hpp"
#include "core/meta.detail.hpp"
#include "core/meta.type_trait.hpp"
#include "core/core.hpp"
#include "core/exception.hpp"
#include "core/guard.hpp"
#include "core/core.cpp"

#include "network/pch.h"
#include "network/net.h"
#include "network/session.base.h"
#include "network/session.server.h"
#include "network/acceptor.h"
#include "network/net.cpp"
#include "network/acceptor.cpp"
#include "network/session.server.cpp"

#include "server/server.hpp"
#include "server/app.h"
#include "server/app.cpp"
#include "server/option.cpp"

int main(int argc, char* argv[])
{
    app::parse_options(argc, argv);
    app::run();
}