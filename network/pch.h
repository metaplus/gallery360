#pragma once

// #include <msgpack.hpp>
#include <WinSock2.h>

#pragma warning(push)
#pragma warning(disable: 4834)
#include <boost/beast.hpp>
#pragma warning(pop)
#include <boost/property_tree/ptree.hpp>
#include <folly/AtomicBitSet.h>

static_assert(
    std::conjunction<
    std::is_same<boost::asio::ip::tcp::socket, boost::asio::basic_stream_socket<boost::asio::ip::tcp>>,
    std::is_same<boost::asio::ip::udp::socket, boost::asio::basic_datagram_socket<boost::asio::ip::udp>>,
    std::is_same<boost::asio::ip::icmp::socket, boost::asio::basic_raw_socket<boost::asio::ip::icmp>>
    >::value);

#include "network/net.hpp"
#include "network/executor_guard.hpp"
#include "network/promise_base.hpp"
#include "network/session_base.h"
// #include "network/context_base.hpp"
// #include "network/send_base.hpp"
// #include "network/recv_base.hpp"
// #include "network/session_element.hpp"
// #include "network/session.hpp"
// #include "network/session_pool.hpp"

// #ifdef _NET_SERVER_PROJECT
#include "network/server.hpp"
#include "network/acceptor.hpp"
// #endif

// #ifdef _NET_CLIENT_PROJECT
#include "network/client.hpp" 
#include "network/connector.hpp"
// #endif
