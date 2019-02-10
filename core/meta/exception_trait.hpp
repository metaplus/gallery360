#pragma once
#include "core/meta/type_trait.hpp"

namespace meta
{
    template<typename Exception>
    struct is_exception : std::disjunction<
        std::is_base_of<std::exception, Exception>,
        std::is_base_of<boost::exception, Exception>> {};

    template <typename Exception>
    struct has_exception : std::disjunction<
        has_if<std::exception, std::is_base_of<std::exception, Exception>>,
        has_if<boost::exception, std::is_base_of<boost::exception, Exception>>> {};

    template<typename Error>
    struct is_error_code : std::disjunction<
        std::is_same<std::error_code, Error>,
        std::is_same<boost::system::error_code, Error>> {};

    template <typename Error>
    struct has_error_code : std::disjunction<
        has_if<std::error_code, std::is_same<std::error_code, Error>>,
        has_if<boost::system::error_code, std::is_same<boost::system::error_code, Error>>> {};
}