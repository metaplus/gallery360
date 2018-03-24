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

core::aborted_error::aborted_error(const std::string_view desc)
    : runtime_error(desc.data())
{}

const char* core::aborted_error::what() const
{
    return what() ? what() : core::type_shortname<decltype(*this)>().data();
}

core::null_pointer_error::null_pointer_error(std::string_view desc)
    : runtime_error(desc.data())
{}

const char* core::null_pointer_error::what() const
{
    return what() ? what() : core::type_shortname<decltype(*this)>().data();
}

core::dangling_pointer_error::dangling_pointer_error(std::string_view desc)
    : runtime_error(desc.data())
{}

const char* core::dangling_pointer_error::what() const
{
    return what() ? what() : core::type_shortname<decltype(*this)>().data();
}

core::not_implemented_error::not_implemented_error(std::string_view desc)
    : logic_error(desc.data())
{}

const char* core::not_implemented_error::what() const
{
    return what() ? what() : core::type_shortname<decltype(*this)>().data();
}