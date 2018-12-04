#pragma once

namespace core
{
    using errinfo_stacktrace = boost::error_info<as_stacktrace_tag, boost::stacktrace::stacktrace>;

    template<typename Exception>
    [[noreturn]] void throw_with_stacktrace(Exception const& exp) {
        static_assert(meta::is_exception<Exception>::value);
        throw boost::enable_error_info(exp)
            << errinfo_stacktrace(boost::stacktrace::stacktrace());
    }

    template<typename Exception>
    void diagnose_stacktrace(Exception const& exp) {
        static_assert(meta::is_exception<Exception>::value);
        if (auto const* stacktrace = boost::get_error_info<errinfo_stacktrace>(exp); stacktrace != nullptr)
            fmt::print(std::cerr, "stacktrace:\n{}\n", *stacktrace);
    }

    inline int inspect_exception(boost::exception const& exp) {
        fmt::print(std::cerr, "boost-exception-what:\n{}\n\n", boost::diagnostic_information(exp));
        diagnose_stacktrace(exp);
        return EXIT_FAILURE;
    }

    inline int inspect_exception(std::exception const& exp) {
        fmt::print(std::cerr, "std-exception-what:\n{}\n\n", exp.what());
        diagnose_stacktrace(exp);
        try {
            std::rethrow_if_nested(exp);
        } catch (std::exception const& exp2) {
            return inspect_exception(exp2);
        } catch (boost::exception const& exp2) {
            return inspect_exception(exp2);
        } catch (...) {
            fmt::print(std::cerr, "exception-caught: nonstandard exception\n\n");
        }
        return EXIT_FAILURE;
    }

    namespace detail
    {
        template<typename Exception>
        const char* message_otherwise_typename(char const* cstr, Exception const*) {
            static_assert(meta::is_exception<Exception>::value);
            static thread_local std::unordered_set<std::string> local_type_name;
            if (!std::string_view{ cstr }.empty())
                return cstr;
            auto const[iterator, success] = local_type_name.emplace(boost::typeindex::type_id<Exception>().pretty_name());
            boost::ignore_unused(success);
            return iterator->c_str();
        }
    }

    struct aborted_error : std::runtime_error
    {
        using runtime_error::runtime_error;
        using runtime_error::operator=;

        const char* what() const override {
            return detail::message_otherwise_typename(what(), this);
        }
    };

    struct null_pointer_error : std::runtime_error
    {
        using runtime_error::runtime_error;
        using runtime_error::operator=;

        const char* what() const override {
            return detail::message_otherwise_typename(what(), this);
        }
    };

    struct dangling_pointer_error : std::runtime_error
    {
        using runtime_error::runtime_error;
        using runtime_error::operator=;

        const char* what() const override {
            return detail::message_otherwise_typename(what(), this);
        }
    };

    struct not_implemented_error : std::logic_error
    {
        using logic_error::logic_error;
        using logic_error::operator=;

        const char* what() const override {
            return detail::message_otherwise_typename(what(), this);
        }
    };

    struct already_exist_error : std::logic_error
    {
        using logic_error::logic_error;
        using logic_error::operator=;

        const char* what() const override {
            return detail::message_otherwise_typename(what(), this);
        }
    };

    struct unreachable_execution_error : std::logic_error
    {
        using logic_error::logic_error;
        using logic_error::operator=;

        const char* what() const override {
            return detail::message_otherwise_typename(what(), this);
        }
    };

    struct stream_drained_error : std::runtime_error
    {
        using runtime_error::runtime_error;
        using runtime_error::operator=;

        const char* what() const override {
            return detail::message_otherwise_typename(what(), this);
        }
    };

    struct bad_request_error : std::runtime_error
    {
        using runtime_error::runtime_error;
        using runtime_error::operator=;

        const char* what() const override {
            return detail::message_otherwise_typename(what(), this);
        }
    };

    struct bad_response_error : std::runtime_error
    {
        using runtime_error::runtime_error;
        using runtime_error::operator=;

        const char* what() const override {
            return detail::message_otherwise_typename(what(), this);
        }
    };

    struct session_closed_error : std::runtime_error
    {
        using runtime_error::runtime_error;
        using runtime_error::operator=;

        const char* what() const override {
            return detail::message_otherwise_typename(what(), this);
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
