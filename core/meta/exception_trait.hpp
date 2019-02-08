#pragma once

namespace meta
{
    template<typename Exception>
    struct is_exception : std::disjunction<
        std::is_base_of<std::exception, Exception>,
        std::is_base_of<boost::exception, Exception>> {};
}