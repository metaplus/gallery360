#pragma once

namespace core
{
    template <typename Exception>
    [[noreturn]] typename std::enable_if<meta::is_exception<Exception>::value>::type
    throw_with_stacktrace(const Exception& exp) {
        using stacktrace_info = boost::error_info<as_stacktrace_tag, boost::stacktrace::stacktrace>;
        throw boost::enable_error_info(exp)
            << stacktrace_info{ boost::stacktrace::stacktrace() };
    }

    namespace detail
    {
        template <typename Exception>
        typename std::enable_if<meta::is_exception<Exception>::value, const char*>::type
        message_otherwise_typename(char const* cstr, const Exception*) {
            static thread_local std::unordered_set<std::string> local_type_name;
            if (!std::string_view{ cstr }.empty()) return cstr;
            auto const [iterator, success] = local_type_name.emplace(boost::typeindex::type_id<Exception>().pretty_name());
            boost::ignore_unused(success);
            return iterator->c_str();
        }
    }

    struct aborted_error : std::runtime_error
    {
        using runtime_error::runtime_error;
        using runtime_error::operator=;

        const char* what() const noexcept override {
            return detail::message_otherwise_typename(runtime_error::what(), this);
        }
    };

    struct null_pointer_error : std::runtime_error
    {
        using runtime_error::runtime_error;
        using runtime_error::operator=;

        const char* what() const noexcept override {
            return detail::message_otherwise_typename(runtime_error::what(), this);
        }
    };

    struct dangling_pointer_error : std::runtime_error
    {
        using runtime_error::runtime_error;
        using runtime_error::operator=;

        const char* what() const noexcept override {
            return detail::message_otherwise_typename(runtime_error::what(), this);
        }
    };

    struct not_implemented_error : std::logic_error
    {
        using logic_error::logic_error;
        using logic_error::operator=;

        const char* what() const noexcept override {
            return detail::message_otherwise_typename(logic_error::what(), this);
        }
    };

    struct already_exist_error : std::logic_error
    {
        using logic_error::logic_error;
        using logic_error::operator=;

        const char* what() const noexcept override {
            return detail::message_otherwise_typename(logic_error::what(), this);
        }
    };

    struct unreachable_execution_error : std::logic_error
    {
        using logic_error::logic_error;
        using logic_error::operator=;

        const char* what() const noexcept override {
            return detail::message_otherwise_typename(logic_error::what(), this);
        }
    };

    struct stream_drained_error : std::runtime_error
    {
        using runtime_error::runtime_error;
        using runtime_error::operator=;

        const char* what() const noexcept override {
            return detail::message_otherwise_typename(runtime_error::what(), this);
        }
    };

    struct bad_request_error : std::runtime_error
    {
        using runtime_error::runtime_error;
        using runtime_error::operator=;

        const char* what() const noexcept override {
            return detail::message_otherwise_typename(runtime_error::what(), this);
        }
    };

    struct bad_response_error : std::runtime_error
    {
        using runtime_error::runtime_error;
        using runtime_error::operator=;

        const char* what() const noexcept override {
            return detail::message_otherwise_typename(runtime_error::what(), this);
        }
    };

    struct session_closed_error : std::runtime_error
    {
        using runtime_error::runtime_error;
        using runtime_error::operator=;

        const char* what() const noexcept override {
            return detail::message_otherwise_typename(runtime_error::what(), this);
        }
    };

    [[noreturn]] inline void throw_unimplemented(std::string message = ""s) {
        throw not_implemented_error{ message };
    }

    [[noreturn]] inline void throw_unreachable(std::string message = ""s) {
        throw unreachable_execution_error{ message };
    }

    [[noreturn]] inline void throw_drained(std::string message = ""s) {
        throw stream_drained_error{ message };
    }

    [[noreturn]] inline void throw_bad_request(std::string message = ""s) {
        throw bad_request_error{ message };
    }
}
