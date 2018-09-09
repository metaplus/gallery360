#pragma once

#define BOOST_CONFIG_SUPPRESS_OUTDATED_MESSAGE
#define BOOST_THREAD_VERSION 4
#define BOOST_FILESYSTEM_NO_DEPRECATED 
#define BOOST_ASIO_NO_DEPRECATED
#define BOOST_ASIO_USE_FUTURE_HPP

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
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <exception>
#include <execution>
#include <filesystem>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <numeric>
#include <optional>
#include <regex>
#include <set>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>
using namespace std::literals;

#ifdef CORE_USE_BOOST_ASIO
#include <boost/asio.hpp>
#endif //CORE_USE_BOOST_ASIO

//#include <boost/asio/buffer.hpp>
//#include <boost/beast/core/flat_buffer.hpp>
//#include <boost/beast/core/multi_buffer.hpp>
#include <boost/container_hash/hash.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/core/null_deleter.hpp>
#include <boost/cstdlib.hpp>
#include <boost/exception/all.hpp>
//#include <boost/hana.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/numeric/conversion/cast.hpp>
//#include <boost/numeric/conversion/converter.hpp>
//#include <boost/property_tree/ptree.hpp>
//#include <boost/property_tree/xml_parser.hpp>
#include <boost/stacktrace.hpp>
#include <boost/thread.hpp>
#include <boost/type_index.hpp>

#ifdef CORE_USE_BOOST_FIBER
#pragma warning(push)
#pragma warning(disable: 4251)
#include <boost/fiber/all.hpp>
#pragma warning(pop)
#endif //CORE_USE_BOOST_FIBER

//using namespace boost::hana::literals;

#pragma warning(push)
#pragma warning(disable:4267 4250)
#define GLOG_NO_ABBREVIATED_SEVERITIES
//#include <folly/AtomicBitSet.h>
//#include <folly/AtomicHashMap.h>
//#include <folly/AtomicLinkedList.h>
//#include <folly/AtomicUnorderedMap.h>
//#include <folly/concurrency/ConcurrentHashMap.h>
//#include <folly/concurrency/DynamicBoundedQueue.h>
//#include <folly/concurrency/UnboundedQueue.h>
//#include <folly/container/Access.h>
#include <folly/container/Array.h>
#include <folly/container/Foreach.h>
//#include <folly/dynamic.h>
//#include <folly/io/IOBuf.h>
//#include <folly/io/IOBufQueue.h>

#ifdef CORE_USE_FOLLY_EXECUTOR
#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/executors/IOThreadPoolExecutor.h>
#include <folly/executors/SerialExecutor.h>
#endif //CORE_USE_FOLLY_EXECUTOR
//#include <folly/executors/task_queue/LifoSemMPMCQueue.h>
//#include <folly/executors/task_queue/UnboundedBlockingQueue.h>
//#include <folly/executors/thread_factory/NamedThreadFactory.h>
//#include <folly/executors/ThreadedExecutor.h>

#ifdef CORE_USE_FOLLY_FIBER
#include <folly/fibers/Fiber.h>
#endif //CORE_USE_FOLLY_FIBER
#include <folly/Function.h>
//#include <folly/futures/Barrier.h>
//#include <folly/futures/Future.h>
//#include <folly/PackedSyncPtr.h>
//#include <folly/ProducerConsumerQueue.h>
#include <folly/small_vector.h>
//#include <folly/stop_watch.h>
#include <folly/Synchronized.h>
//#include <folly/SynchronizedPtr.h>
#include <folly/Uri.h>
using namespace folly::literals;
#pragma warning(pop)

//#include <spdlog/spdlog.h>
//#include <spdlog/fmt/ostr.h>

#include <fmt/format.h>
#include <fmt/ostream.h>
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
#include "concurrency/async_chain.hpp"
#include "concurrency/barrier.hpp"
#include "concurrency/latch.hpp"
#include "concurrency/synchronize.hpp"
using namespace core::literals;

//#ifdef CORE_USE_FMT
//#ifdef _DEBUG
//#pragma comment(lib, "fmtd")
//#else
//#pragma comment(lib, "fmt")
//#endif  // _DEBUG
//#endif

//#ifdef CORE_USE_TBB
//#include <tbb/tbb.h>
//#ifdef _DEBUG
//#pragma comment(lib, "tbb_debug")
//#else
//#pragma comment(lib, "tbb")
//#endif  // _DEBUG
//#endif  // CORE_USE_TBB
