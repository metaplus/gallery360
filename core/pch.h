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
#include <cassert>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <exception>
#include <execution>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>
using namespace std::literals;

#include <boost/asio/buffer.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/multi_buffer.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/container_hash/hash_fwd.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/core/null_deleter.hpp>
#include <boost/cstdlib.hpp>
#include <boost/exception/all.hpp>
//#include <boost/hana.hpp>
#include <boost/logic/tribool_fwd.hpp>
#include <boost/stacktrace.hpp>
#include <boost/type_index.hpp>

//using namespace boost::hana::literals;
#define GLOG_NO_ABBREVIATED_SEVERITIES
#pragma warning(push)
#pragma warning(disable:4267 4250)
//#include <folly/AtomicHashMap.h>
//#include <folly/AtomicBitSet.h>
//#include <folly/AtomicLinkedList.h>
//#include <folly/AtomicUnorderedMap.h>
#pragma warning(disable:4200 4305 4244)
#include <folly/concurrency/ConcurrentHashMap.h>
#pragma warning(pop)
//#include <folly/concurrency/DynamicBoundedQueue.h>
//#include <folly/concurrency/UnboundedQueue.h>
//#include <folly/container/Access.h>
#include <folly/container/Array.h>
#include <folly/container/Foreach.h>
#include <folly/container/Iterator.h>
#include <folly/executors/Async.h>
#include <folly/executors/GlobalExecutor.h>
#include <folly/executors/ThreadedExecutor.h>
#include <folly/executors/ThreadPoolExecutor.h>

#include <folly/Function.h>
#include <folly/futures/Barrier.h>
#include <folly/futures/Future.h>
#include <folly/futures/FutureSplitter.h>
#include <folly/Lazy.h>
#include <folly/MoveWrapper.h>
//#include <folly/PackedSyncPtr.h>
//#include <folly/ProducerConsumerQueue.h>
#include <folly/Random.h>
#include <folly/Synchronized.h>
//#include <folly/SynchronizedPtr.h>
#include <folly/Uri.h>
#pragma warning(pop)

#include <nlohmann/json.hpp>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <spdlog/logger.h>

//#include <fmt/time.h>
//#include <fmt/ranges.h>
using namespace fmt::literals;

#include "meta/meta.hpp"
#include "meta/detail.hpp"
#include "meta/type_trait.hpp"
#include "meta/function_trait.hpp"
#include "meta/member_function_trait.hpp"
#include "core/core.hpp"
#include "core/exception.hpp"
#include "core/guard.hpp"
#include "core/verify.hpp"
//#include "concurrency/async_chain.hpp"
//#include "concurrency/barrier.hpp"
//#include "concurrency/latch.hpp"
//#include "concurrency/synchronize.hpp"
using namespace core::literals;
