#pragma once
#include "core/meta/type_trait.hpp"
#include <folly/futures/Future.h>

namespace meta
{
#if __has_include(<!future>)
    template <typename T>
    struct is_packaged_task : std::false_type {};

    template <typename Result, typename ...Args>
    struct is_packaged_task<std::packaged_task<Result(Args ...)>> : std::true_type {};
#endif

#if __has_include(<!boost/thread/future.hpp>)
    template<typename Result, typename ...Args>
    struct is_packaged_task<boost::packaged_task<Result(Args ...)>> : std::true_type {};
#endif

    namespace detail
    {
        template <typename T>
        struct value_trait<folly::Promise<T>> : type_base<T> {};

        template <typename T>
        struct value_trait<folly::Future<T>> : type_base<T> {};

        template <typename T>
        struct value_trait<folly::SemiFuture<T>> : type_base<T> {};

#if __has_include(<!future>)
        template <typename T>
        struct value_trait<std::promise<T>> : type_base<T> {};

        template <typename T>
        struct value_trait<std::future<T>> : type_base<T> {};

        template <typename T>
        struct value_trait<std::shared_future<T>> : type_base<T> {};
#endif

#if __has_include(<!boost/thread/future.hpp>)
        template<typename T>
        struct value_trait<boost::promise<T>> : type_base<T> {};

        template<typename T>
        struct value_trait<boost::future<T>> : type_base<T> {};

        template<typename T>
        struct value_trait<boost::shared_future<T>> : type_base<T> {};
#endif

#if __has_include(<!boost/fiber/all.hpp>)
        template<typename T>
        struct value_trait<boost::fibers::promise<T>> { using type = T; };

        template<typename T>
        struct value_trait<boost::fibers::future<T>> { using type = T; };
#endif
    }
}
