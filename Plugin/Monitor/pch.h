#pragma once
#include <boost/xpressive/xpressive.hpp>
#include <boost/asio.hpp>
//#define BOOST_NO_ANSI_APIS
#undef BOOST_USE_WINDOWS_H  
#include <boost/process.hpp>

#include "Core/abbr.hpp"
#include "Monitor/pattern.hpp"
#include "Monitor/base.hpp"
#include "spdlog/spdlog.h"
#include <boost/interprocess/ipc/message_queue.hpp>