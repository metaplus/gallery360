#pragma once
/**
 * @note type alias and namespace facility for .cpp source code, included
 * in precompiling relavent headers
 */
#if __has_include(<chrono_literal>) ||\
    __has_include(<string>)||\
    __has_include(<string_view>)
using namespace std::literals;
#endif 
#if __has_include(<filesystem>)
namespace filesystem=std::experimental::filesystem;
#endif 
#if __has_include(<chrono>)
using namespace std::chrono; 
#endif 

#if 0   //cumbersome MSVC, typical case here not working as expect
#if __has_include(<boost/asio.hpp>)
#define ABBR_HAS_ASIO 1
#else
#define ABBR_HAS_ASIO 0
#endif
//#if __has_include(<boost/process.hpp>) && 0
//namespace process=boost::process;
//namespace this_process=boost::this_process;
//#endif 
#if ABBR_HAS_ASIO
namespace asio=boost::asio;
#endif
#endif