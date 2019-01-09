#pragma once

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable: 4834)
#include <WinSock2.h>
#endif

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/container/small_vector.hpp>

#ifdef _WIN32
#pragma warning(pop)
#include "network/net.hpp"
#include "network/session.base.h"
#endif
