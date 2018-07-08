#pragma once

namespace core
{
    namespace detail
    {
        [[noreturn]] inline void verify_one(std::nullptr_t const&)
        {
            throw_with_stacktrace(null_pointer_error{ "null pointer" });
        }

        template<typename Pointee>
        void verify_one(Pointee* const& ptr)
        {
            if (!ptr)
                throw_with_stacktrace(dangling_pointer_error{
                    "dangling pointer, pointer type: " + boost::typeindex::type_id<Pointee>().pretty_name()
                                      });
        }

        template<typename Arithmetic,
            typename = boost::hana::when<std::is_arithmetic<Arithmetic>::value>>
            void verify_one(Arithmetic const& number)
        {
            if (std::is_signed<Arithmetic>::value && number < 0)
                throw_with_stacktrace(std::out_of_range{ "negative value" });
        }

        inline void verify_one(bool const& condition)
        {
            if (!condition)
                throw_with_stacktrace(std::logic_error{ "condition false" });
        }
    }

    template<typename ...Predicates>
    void verify(Predicates const& ...preds)
    {
        (..., detail::verify_one(preds));
    }
}
