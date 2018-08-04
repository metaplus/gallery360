#pragma once

namespace core
{
    using errinfo_stacktrace = boost::error_info<as_stacktrace_t, boost::stacktrace::stacktrace>;

    template<typename Exception>
    [[noreturn]] void throw_with_stacktrace(Exception const& exp)
    {
        static_assert(meta::is_exception<Exception>::value);
        throw boost::enable_error_info(exp)
            << errinfo_stacktrace(boost::stacktrace::stacktrace());
    }

    template<typename Exception>
    void diagnose_stacktrace(Exception const& exp)
    {
        static_assert(meta::is_exception<Exception>::value);
        if (auto const* stacktrace = boost::get_error_info<errinfo_stacktrace>(exp); stacktrace != nullptr)
            fmt::print(std::cerr, "stacktrace:\n{}\n", *stacktrace);
    }

    inline int inspect_exception(boost::exception const& exp)
    {
        fmt::print(std::cerr, "boost-exception-what:\n{}\n\n", boost::diagnostic_information(exp));
        diagnose_stacktrace(exp);
        return EXIT_FAILURE;
    }

    inline int inspect_exception(std::exception const& exp)
    {
        fmt::print(std::cerr, "std-exception-what:\n{}\n\n", exp.what());
        diagnose_stacktrace(exp);
        try
        {
            std::rethrow_if_nested(exp);
        }
        catch (std::exception const& exp2)
        {
            return inspect_exception(exp2);
        }
        catch (boost::exception const& exp2)
        {
            return inspect_exception(exp2);
        }
        catch (...)
        {
            fmt::print(std::cerr, "exception-caught: nonstandard exception\n\n");
        }
        return EXIT_FAILURE;
    }

    namespace detail
    {
        template<typename Exception>
        char const* message_otherwise_typename(char const* cstr, Exception const*)
        {
            static_assert(meta::is_exception<Exception>::value);
            static thread_local std::unordered_set<std::string> local_type_name;
            if (!std::string_view{ cstr }.empty())
                return cstr;
            auto const [iterator, success] = local_type_name.emplace(boost::typeindex::type_id<Exception>().pretty_name());
            boost::ignore_unused(success);
            return iterator->c_str();
        }
    }

    class aborted_error : protected std::runtime_error
    {
    public:
        using runtime_error::runtime_error;
        using runtime_error::operator=;

        char const* what() const override
        {
            return detail::message_otherwise_typename(what(), this);
        }
    };

    class null_pointer_error : protected std::runtime_error
    {
    public:
        using runtime_error::runtime_error;
        using runtime_error::operator=;

        char const* what() const override
        {
            return detail::message_otherwise_typename(what(), this);
        }
    };

    class dangling_pointer_error : protected std::runtime_error
    {
    public:
        using runtime_error::runtime_error;
        using runtime_error::operator=;

        char const* what() const override
        {
            return detail::message_otherwise_typename(what(), this);
        }
    };

    class not_implemented_error : protected std::logic_error
    {
    public:
        using logic_error::logic_error;
        using logic_error::operator=;

        char const* what() const override
        {
            return detail::message_otherwise_typename(what(), this);
        }
    };

    class already_exist_error : protected std::logic_error
    {
    public:
        using logic_error::logic_error;
        using logic_error::operator=;

        char const* what() const override
        {
            return detail::message_otherwise_typename(what(), this);
        }
    };

    class unreachable_execution_branch : protected std::logic_error
    {
    public:
        using logic_error::logic_error;
        using logic_error::operator=;

        char const* what() const override
        {
            return detail::message_otherwise_typename(what(), this);
        }
    };
}
