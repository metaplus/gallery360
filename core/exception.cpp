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

core::force_exit_exception::force_exit_exception(const std::string_view desc)
    : description_(desc)
{}

const char* core::force_exit_exception::what() const
{
    return description_.empty() ? "force_exit_exception" : description_.data();
}
