#pragma once
#include <msgpack.hpp>
#include <WinSock2.h>
#pragma warning(push)
#pragma warning(disable: 4834)
#include <boost/asio.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/asio/buffer.hpp>
#pragma warning(pop)

#include "network/session.hpp"
#include "network/client.hpp" 