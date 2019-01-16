#pragma once

#define BOOST_CONFIG_SUPPRESS_OUTDATED_MESSAGE
#define BOOST_THREAD_VERSION 4
#define BOOST_FILESYSTEM_NO_DEPRECATED 
#define BOOST_ASIO_NO_DEPRECATED
#define BOOST_ASIO_USE_FUTURE_HPP

#define _CRT_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#define _ENABLE_EXTENDED_ALIGNED_STORAGE

#define STRING_IMPL(x) #x
#define STRING(x) STRING_IMPL(x)

#include <algorithm>
#include <any>
#include <atomic>
#include <bitset>
#include <cassert>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <numeric>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#ifdef _WIN32
#include <execution>
#include <memory_resource>
#endif

using namespace std::literals;
//using namespace boost::hana::literals;

#define GLOG_NO_ABBREVIATED_SEVERITIES
#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable:4267 4250)
#endif
//#include <folly/AtomicHashMap.h>
//#include <folly/AtomicBitSet.h>
//#include <folly/AtomicLinkedList.h>
//#include <folly/AtomicUnorderedMap.h>
//#include <folly/ConcurrentSkipList.h>
//#include <folly/concurrency/ConcurrentHashMap.h>
#ifdef _WIN32
#pragma warning(disable:4244)
#endif
#include <folly/container/Foreach.h>
#ifdef _WIN32
#pragma warning(pop)
#endif
//#include <folly/concurrency/DynamicBoundedQueue.h>
//#include <folly/concurrency/UnboundedQueue.h>
#include <folly/container/Array.h>
#include <folly/container/Iterator.h>
#include <folly/executors/Async.h>
#include <folly/executors/GlobalExecutor.h>
#include <folly/executors/SerialExecutor.h>
#include <folly/executors/ThreadedExecutor.h>
#include <folly/executors/ThreadPoolExecutor.h>
#include <folly/Function.h>
#include <folly/futures/Barrier.h>
#include <folly/futures/Future.h>
#include <folly/futures/FutureSplitter.h>
#include <folly/Lazy.h>
#include <folly/MoveWrapper.h>
//#include <folly/MPMCQueue.h>
//#include <folly/PriorityMPMCQueue.h>
//#include <folly/ProducerConsumerQueue.h>
#include <folly/Random.h>
//#include <folly/Synchronized.h>
//#include <folly/SynchronizedPtr.h>
#include <folly/Uri.h>

#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/basic_endpoint.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/multi_buffer.hpp>
#include <boost/circular_buffer_fwd.hpp>
#include <boost/container/container_fwd.hpp>
#include <boost/container_hash/hash_fwd.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/core/null_deleter.hpp>
#include <boost/cstdlib.hpp>
#include <boost/exception/all.hpp>
#include <boost/logic/tribool_fwd.hpp>
#include <boost/multi_index/hashed_index_fwd.hpp>
#include <boost/multi_index/identity_fwd.hpp>
#include <boost/multi_index/ordered_index_fwd.hpp>
#include <boost/multi_index/random_access_index_fwd.hpp>
#include <boost/multi_index/ranked_index_fwd.hpp>
#include <boost/multi_index/sequenced_index_fwd.hpp>
#include <boost/multi_index_container_fwd.hpp>
#include <boost/stacktrace.hpp>
#include <boost/type_index.hpp>
//#include <boost/hana.hpp>

#include <nlohmann/json.hpp>

#include <fmt/format.h>
#include <fmt/ostream.h>

#define SPDLOG_FMT_EXTERNAL
#include <spdlog/logger.h>

using namespace fmt::literals;

#ifdef _WIN32
#include "core/meta.hpp"
#include "core/detail.hpp"
#include "core/type_trait.hpp"
#include "core/function_trait.hpp"
#include "core/member_function_trait.hpp"
#include "core/core.hpp"
#include "core/exception.hpp"
#include "core/guard.hpp"
#include "core/verify.hpp"

using namespace core::literals;
#endif