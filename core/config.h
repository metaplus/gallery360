#pragma once

// Visual Studio
#define _CRT_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#define _ENABLE_EXTENDED_ALIGNED_STORAGE

#define STRING_IMPL(x) #x
#define STRING(x) STRING_IMPL(x)

// Boost
#define BOOST_CONFIG_SUPPRESS_OUTDATED_MESSAGE
#define BOOST_THREAD_VERSION 4
#define BOOST_FILESYSTEM_NO_DEPRECATED 
#define BOOST_ASIO_NO_DEPRECATED
#define BOOST_ASIO_USE_FUTURE_HPP

// Spdlog
#define SPDLOG_FMT_EXTERNAL 

// Folly
#define GLOG_NO_ABBREVIATED_SEVERITIES
