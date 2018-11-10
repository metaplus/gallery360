#pragma once

// #include <msgpack.hpp>
#include <WinSock2.h>
#include <boost/asio.hpp>
#pragma warning(push)
#pragma warning(disable: 4834)
#include <boost/beast.hpp>
#pragma warning(pop)
#include <folly/AtomicBitSet.h>

#include "network/net.hpp"
#include "network/executor_guard.hpp"
//#include "network/promise_base.hpp"
#include "network/session_base.h"
// #include "network/context_base.hpp"
// #include "network/send_base.hpp"
// #include "network/recv_base.hpp"
// #include "network/session_element.hpp"
// #include "network/session.hpp"
// #include "network/session_pool.hpp"

// #ifdef _NET_SERVER_PROJECT
//#include "network/server.hpp"
//#include "network/acceptor.hpp"
// #endif

// #ifdef _NET_CLIENT_PROJECT
//#include "network/client.hpp" 
//#include "network/connector.hpp"
// #endif
