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
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS //tbb usage

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
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <type_traits>
#include <variant>
#include <vector>
//#include <boost/thread.hpp>
//#include <boost/thread/scoped_thread.hpp>
//#include <boost/thread/future.hpp>
//#include <boost/assert.hpp>
//#include <boost/archive/binary_iarchive.hpp>
//#include <boost/archive/binary_oarchive.hpp>
//#include <boost/convert.hpp>
//#include <boost/convert/spirit.hpp>
//#include <boost/convert/stream.hpp>
//#include <boost/convert/lexical_cast.hpp>
//#include <boost/convert/strtol.hpp>
//#include <boost/archive/text_iarchive.hpp>
//#include <boost/archive/text_oarchive.hpp>
#include <boost/core/null_deleter.hpp>
#include <boost/cstdlib.hpp>
//#include <boost/lexical_cast.hpp>                     //prefer std::from_chars/to_chars
//#include <boost/serialization/array.hpp>
//#include <boost/serialization/assume_abstract.hpp>
//#include <boost/serialization/base_object.hpp>
//#include <boost/serialization/binary_object.hpp>      
//#include <boost/serialization/string.hpp>
//#include <boost/serialization/utility.hpp>
//#include <boost/serialization/vector.hpp>
//#include <boost/serialization/version.hpp>
#include <fmt/container.h>    
#include <fmt/format.h>                  
#include <fmt/ostream.h>
#include <fmt/string.h>
#include <fmt/time.h>
#include <tbb/cache_aligned_allocator.h>
#include <tbb/scalable_allocator.h>
#include <tbb/tbb.h>
#include <tbb/tbb_allocator.h>
//#include <spdlog/spdlog.h>
//#include <spdlog/fmt/ostr.h>
//#include <termcolor/termcolor.hpp>
using namespace std::literals;
using namespace fmt::literals;
#include "Core/base.hpp"
#include "Core/revokable.hpp"
#include "Core/guard.h"

#define STRING2(x) #x  
#define STRING(x) STRING2(x)  

#ifdef _WIN32
#ifndef NDEBUG
#pragma comment(lib,"tbb_debug")
#pragma comment(lib,"Debug/fmt")
#else
#pragma comment(lib,"tbb")
#pragma comment(lib,"Release/fmt")
#endif  //NDEBUG
#endif  //_WIN32


