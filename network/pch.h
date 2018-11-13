#pragma once

#include <WinSock2.h>
#include <boost/asio.hpp>
#pragma warning(push)
#pragma warning(disable: 4834)
#include <boost/beast.hpp>
#pragma warning(pop)
#include <folly/AtomicBitSet.h>

#include "network/net.hpp"
#include "network/session_base.h"
