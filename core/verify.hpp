#pragma once

namespace core
{
    namespace detail
    {
        _FORCEINLINE void verify_one(std::nullptr_t const&)
        {
            throw_with_stacktrace(null_pointer_error{ "null pointer" });
        }

        template<typename Pointee>
        _FORCEINLINE void verify_one(Pointee* const& ptr)
        {
            if (ptr != nullptr) return;
            throw_with_stacktrace(dangling_pointer_error{
                    "dangling pointer, pointer type: " + boost::typeindex::type_id<Pointee>().pretty_name() });
        }

        template<typename Arithmetic>
        _FORCEINLINE void verify_one(Arithmetic const& number, 
                                     typename std::enable_if<std::is_arithmetic<Arithmetic>::value>::type* = nullptr)
        {
            if (std::is_unsigned<Arithmetic>::value || number >= 0) return;
            throw_with_stacktrace(std::out_of_range{ "negative value" });
        }

        _FORCEINLINE void verify_one(bool const& condition)
        {
            if (condition) return;
            throw_with_stacktrace(std::logic_error{ "condition false" });
        }
    }

    template<typename ...Types>
    _FORCEINLINE void verify(Types const& ...predicates)
    {
        // TODO: sfinae for boolean convertible case
        (..., detail::verify_one(predicates));
    };
}
