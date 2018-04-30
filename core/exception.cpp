#include "stdafx.h"
#include "exception.h"

int core::inspect_exception(const std::exception & e)
{
    std::cerr << e.what() << ' ';
    try
    {
        std::rethrow_if_nested(e);
    }
    catch (const std::exception& another)
    {
        return inspect_exception(another);
    }
    catch (...)
    {
        std::cerr << "nonstandard exception";
    }
    std::cerr << std::endl;
    return EXIT_FAILURE;
}

namespace
{
    template<typename T>
    const char* deduce_error_cstring(const char* cstr, const T* = nullptr)
    {
        static thread_local std::string local_type_name;
        return std::strlen(cstr) != 0 ? cstr : (local_type_name = core::type_shortname<std::remove_cv_t<T>>()).c_str();
    }
}

core::aborted_error::aborted_error(const std::string_view desc)
    : runtime_error(desc.data())
{}

const char* core::aborted_error::what() const
{
    return deduce_error_cstring(what(), this);
}

core::null_pointer_error::null_pointer_error(std::string_view desc)
    : runtime_error(desc.data())
{}

const char* core::null_pointer_error::what() const
{
    return deduce_error_cstring(what(), this);
}

core::dangling_pointer_error::dangling_pointer_error(std::string_view desc)
    : runtime_error(desc.data())
{}

const char* core::dangling_pointer_error::what() const
{
    return deduce_error_cstring(what(), this);
}

core::not_implemented_error::not_implemented_error(std::string_view desc)
    : logic_error(desc.data())
{}

const char* core::not_implemented_error::what() const
{
    return deduce_error_cstring(what(), this);
}

core::already_exist_error::already_exist_error(std::string_view desc)
    : logic_error(desc.data())
{}

const char* core::already_exist_error::what() const
{
    return deduce_error_cstring(what(), this);
}
