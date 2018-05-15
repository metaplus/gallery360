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

// #define _SILENCE_PARALLEL_ALGORITHMS_EXPERIMENTAL_WARNING   // <execution>

#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS

// #undef min
// #undef max

#define STRING_IMPL(x) #x  
#define STRING(x) STRING_IMPL(x)  

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
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>
#include <filesystem>

#include <boost/core/null_deleter.hpp>
#include <boost/cstdlib.hpp>
#include <boost/fiber/all.hpp>

using namespace std::literals;

#ifdef CORE_USE_COROUTINE
#include <experimental/coroutine>
#endif

#include "meta/meta.hpp"
#include "meta/detail.hpp"
#include "meta/type_trait.hpp"
#include "meta/function_trait.hpp"
#include "meta/member_function_trait.hpp"
#include "core/exception.h"
#include "core/core.hpp"
#include "core/guard.h"
#include "core/verify.hpp"

#ifdef CORE_USE_GRAPH
#include "core/graph.hpp"
#endif  // CORE_USE_GRAPH

#include "concurrency/synchronize.h"
#include "concurrency/latch.hpp"
#include "concurrency/barrier.hpp"
#include "concurrency/async_chain.hpp"

using namespace core::literals;

#ifdef CORE_USE_FMTLIB

#include <fmt/format.h>
#include <fmt/container.h>
#include <fmt/ostream.h>
#include <fmt/string.h>
#include <fmt/time.h>

using namespace fmt::literals;

#ifdef _DEBUG
#pragma comment(lib, "Debug/fmt")
#else
#pragma comment(lib, "Release/fmt")
#endif  // _DEBUG

#endif  // CORE_USE_FMTLIB