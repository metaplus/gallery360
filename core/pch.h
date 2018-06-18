#pragma once

#ifdef _DEBUG
#define BOOST_MULTI_INDEX_ENABLE_INVARIANT_CHECKING
#define BOOST_MULTI_INDEX_ENABLE_SAFE_MODE
#endif

#define BOOST_CONFIG_SUPPRESS_OUTDATED_MESSAGE
#define BOOST_THREAD_VERSION 4
#define BOOST_FILESYSTEM_NO_DEPRECATED 

#define _CRT_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS

#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS

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
// #include <boost/fiber/all.hpp>
#include <boost/container_hash/hash.hpp>

#ifdef CORE_USE_FUSION
#include <boost/fusion/support.hpp>
#include <boost/fusion/iterator.hpp>
#include <boost/fusion/sequence.hpp>
#include <boost/fusion/container.hpp>
#include <boost/fusion/view.hpp>
#include <boost/fusion/adapted.hpp>
#include <boost/fusion/algorithm.hpp>
#include <boost/fusion/tuple.hpp>
#include <boost/fusion/functional.hpp>
// #include <boost/fusion/include/hash.hpp>
#endif

#include <boost/type_index.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <boost/numeric/conversion/converter.hpp>
#include <boost/thread.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

using namespace std::literals;

#ifdef CORE_USE_COROUTINE
#include <experimental/coroutine>
#endif

#ifndef CORE_NOUSE_FMTLIB
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <fmt/time.h>
// #include <fmt/ranges.h>
using namespace fmt::literals;
#endif  // ndef CORE_NOUSE_FMTLIB

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

#include "concurrency/synchronize.hpp"
#include "concurrency/latch.hpp"
#include "concurrency/barrier.hpp"
#include "concurrency/async_chain.hpp"

using namespace core::literals;

#ifdef _DEBUG
#pragma comment(lib, "Debug/fmtd")
#else
#pragma comment(lib, "Release/fmt")
#endif  // _DEBUG

#ifdef CORE_USE_TBB
#include <tbb/tbb.h>
#ifdef _DEBUG
#pragma comment(lib, "tbb_debug")
#else
#pragma comment(lib, "tbb")
#endif  // _DEBUG
#endif  // CORE_USE_TBB