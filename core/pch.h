#pragma once

#ifdef _DEBUG
#define BOOST_MULTI_INDEX_ENABLE_INVARIANT_CHECKING
#define BOOST_MULTI_INDEX_ENABLE_SAFE_MODE
#endif 
#define BOOST_CONFIG_SUPPRESS_OUTDATED_MESSAGE
#define BOOST_THREAD_VERSION 4
#define BOOST_FILESYSTEM_NO_DEPRECATED 
#define BOOST_USE_WINDOWS_H   

#define _CRT_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS

#define _SILENCE_PARALLEL_ALGORITHMS_EXPERIMENTAL_WARNING   // <execution>

#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS             // <tbb/tbb.h>          

#undef min          //abolish vicious macros from <windows.h>, otherwise causing naming collision against STL
#undef max          //another tolerable solution appears like #define max_RESUME max #undef max ... #define max max_RESUME

#define STRING2(x) #x  
#define STRING(x) STRING2(x)  

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
#include <iomanip>
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
#include <sstream>
#include <thread>
#include <type_traits>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>
#include <boost/core/null_deleter.hpp>
#include <boost/cstdlib.hpp>

using namespace std::literals;

//#if __has_include(<filesystem>)
#include <filesystem>
namespace filesystem = std::experimental::filesystem;
//#elif __has_include(<boost/filesystem.hpp>)
//#include <boost/filesystem.hpp>
//namespace filesystem = boost::filesystem;
//#endif

#include "meta/meta.hpp"
#include "meta/detail/type_trait.hpp"
#include "meta/type_trait.hpp"
#include "core/exception.h"
#include "core/core.h"
#include "core/guard.h"
#include "core/verify.hpp"
#include "core/graph.hpp"
#include "concurrency/synchronize.h"
#include "concurrency/barrier.h"
#include "concurrency/async_chain.h"

#ifdef CORE_USE_FMTLIB
#include <fmt/format.h>
#include <fmt/container.h>
#include <fmt/ostream.h>
#include <fmt/string.h>
#include <fmt/time.h>
#ifdef _DEBUG
#pragma comment(lib, "Debug/fmt")
#else
#pragma comment(lib, "Release/fmt")
#endif // _DEBUG
#endif // CORE_USE_FMTLIB

namespace std
{
    namespace filesystem = std::experimental::filesystem;
}