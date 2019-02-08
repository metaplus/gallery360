#pragma once
#include <boost/exception/all.hpp>
#include <boost/stacktrace.hpp>
#include <boost/system/error_code.hpp>
#include <boost/type_index.hpp>
#include "core/meta/exception_trait.hpp"

namespace core
{
    using errinfo_stacktrace = boost::error_info<struct stacktrace_tag, boost::stacktrace::stacktrace>;
    using errinfo_code = boost::error_info<struct code_tag, boost::system::error_code>;
    using errinfo_message = boost::error_info<struct message_tag, std::string>;

    template <typename Exception, typename ...Infos>
    [[noreturn]] typename std::enable_if<meta::is_exception<Exception>::value>::type
    throw_with_infos(Exception exception, Infos ... info) {
        boost::throw_exception(
            (boost::enable_error_info(exception) << ... << info));
    }

    template <typename Exception>
    [[noreturn]] typename std::enable_if<meta::is_exception<Exception>::value>::type
    throw_with_stacktrace(Exception exception) {
        throw_with_infos(
            exception,
            boost::errinfo_type_info_name{ boost::typeindex::type_id<Exception>().pretty_name() },
            errinfo_stacktrace{ boost::stacktrace::stacktrace{} });
    }

    template <typename Exception = void>
    struct exception_base : virtual std::exception, virtual boost::exception
    {
        [[noreturn]] static void throw_directly() {
            boost::throw_exception(Exception{});
        }

        [[noreturn]] static void throw_with_message(std::string_view message) {
            if (message.empty()) {
                throw_directly();
            }
            throw_with_infos(errinfo_message{ std::string{ message } });
        }

        template <typename ...Args>
        [[noreturn]] static void throw_with_message(
            std::string_view format, Args&& ...args) {
            throw_with_message(fmt::format(format, std::forward<Args>(args)...));
        }

        [[noreturn]] static void throw_in_function(std::string_view function) {
            if (function.empty()) {
                throw_directly();
            }
            throw_with_infos(
                boost::errinfo_type_info_name{ boost::typeindex::type_id<Exception>().pretty_name() },
                boost::errinfo_api_function{ function.data() });
        }

        template <typename ...Infos>
        [[noreturn]] static void throw_with_infos(Infos ...info) {
            if constexpr (sizeof...(Infos) > 0) {
                core::throw_with_infos(Exception{}, info...);
            }
            throw_directly();
        }
    };

    template <>
    struct exception_base<void> {};

    struct aborted_error : virtual exception_base<aborted_error> {};
    struct pointer_error : virtual exception_base<> {};
    struct null_pointer_error : virtual exception_base<null_pointer_error>,
                                virtual pointer_error {};
    struct dangling_pointer_error : virtual exception_base<dangling_pointer_error>,
                                    virtual pointer_error {};
    struct duplicate_error : virtual exception_base<duplicate_error> {};
    struct not_valid_error : virtual exception_base<not_valid_error> {};
    struct not_implemented_error : virtual exception_base<not_implemented_error> {};
    struct not_reachable_error : virtual exception_base<not_reachable_error> {};
    struct session_error : virtual exception_base<> {};
    struct stream_drained_error : virtual exception_base<stream_drained_error>,
                                  virtual session_error {};
    struct bad_request_error : virtual exception_base<bad_request_error>,
                               virtual session_error {};
    struct bad_response_error : virtual exception_base<bad_response_error>,
                                virtual session_error {};
    struct session_closed_error : virtual exception_base<session_closed_error>,
                                  virtual session_error {};
}
