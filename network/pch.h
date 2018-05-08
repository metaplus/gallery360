#pragma once

#include <msgpack.hpp>
#include <WinSock2.h>

#pragma warning(push)
#pragma warning(disable: 4834)

#define BOOST_ASIO_NO_DEPRECATED
#define BOOST_ASIO_USE_FUTURE_HPP
// #define BOOST_ASIO_DISABLE_BUFFER_DEBUGGING

#include <boost/asio.hpp>

static_assert(std::conjunction<
    std::is_same<boost::asio::ip::tcp::socket, boost::asio::basic_stream_socket<boost::asio::ip::tcp>>,
    std::is_same<boost::asio::ip::udp::socket, boost::asio::basic_datagram_socket<boost::asio::ip::udp>>,
    std::is_same<boost::asio::ip::icmp::socket, boost::asio::basic_raw_socket<boost::asio::ip::icmp>>
>::value);

#pragma warning(pop)

#include "network/session_element.hpp"
#include "network/session.hpp"
#include "network/session_pool.hpp"
#include "network/client.hpp" 
#include "network/server.hpp"