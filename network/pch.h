#pragma once
#include <msgpack.hpp>
#include <WinSock2.h>

#pragma warning(push)
#pragma warning(disable: 4834)

#define BOOST_ASIO_NO_DEPRECATED
#define BOOST_ASIO_USE_FUTURE_HPP

#include <boost/asio/use_future.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio.hpp>

#pragma warning(pop)

#include "network/session.hpp"
#include "network/client.hpp" 