// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

#include "targetver.h"

// TODO: reference additional headers your program requires here
#define CORE_USE_BOOST_ASIO
//#define CORE_USE_BOOST_FIBER
#include "core/pch.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "network/pch.h"
#include "network/server.hpp"
#include "network/acceptor.hpp"
#include "network/client.hpp" 
#include "network/connector.hpp"

static_assert(
    std::conjunction<
    std::is_same<boost::asio::ip::tcp::socket, boost::asio::basic_stream_socket<boost::asio::ip::tcp>>,
    std::is_same<boost::asio::ip::udp::socket, boost::asio::basic_datagram_socket<boost::asio::ip::udp>>,
    std::is_same<boost::asio::ip::icmp::socket, boost::asio::basic_raw_socket<boost::asio::ip::icmp>>
    >::value);
