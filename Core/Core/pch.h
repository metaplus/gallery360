#pragma once
#ifndef NDEBUG
#define BOOST_MULTI_INDEX_ENABLE_INVARIANT_CHECKING
#define BOOST_MULTI_INDEX_ENABLE_SAFE_MODE
#endif  //NDEBUG
#define BOOST_CONFIG_SUPPRESS_OUTDATED_MESSAGE
#define BOOST_THREAD_VERSION 4
#define BOOST_FILESYSTEM_NO_DEPRECATED 
#define BOOST_USE_WINDOWS_H   
#define _SCL_SECURE_NO_WARNINGS
#define _SILENCE_PARALLEL_ALGORITHMS_EXPERIMENTAL_WARNING   // <execution>
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS             // tbb usage

#include <algorithm>
#include <any>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <exception>
#include <execution>
#include <fstream>
#include <future>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <numeric>
#include <optional>
#include <regex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <type_traits>
#include <variant>
#include <vector>
#include <boost/core/null_deleter.hpp>
#include <boost/cstdlib.hpp>
#include <tbb/tbb.h>
#include <fmt/container.h>    
#include <fmt/format.h>                  
#include <fmt/ostream.h>
#include <fmt/string.h>
#include <fmt/time.h>

using namespace std::literals;
using namespace fmt::literals;

//#if __has_include(<filesystem>)
#include <filesystem>
namespace filesystem = std::experimental::filesystem;
//#elif __has_include(<boost/filesystem.hpp>)
//#include <boost/filesystem.hpp>
//namespace filesystem = boost::filesystem;
//#endif

#include "Core/meta.hpp"
#include "Core/base.hpp"
#include "Core/revokable.hpp"
#include "Core/guard.h"
#include "Core/sync.h"
#define STRING2(x) #x  
#define STRING(x) STRING2(x)  

#ifdef _WIN32
#ifndef NDEBUG
#pragma comment(lib,"tbb_debug")
#pragma comment(lib,"Debug/fmt")
#else
#pragma comment(lib,"tbb")
#pragma comment(lib,"Release/fmt")
#endif  // NDEBUG
#endif  // _WIN32