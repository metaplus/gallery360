#pragma once

namespace core
{
    template<typename Scalar>
    constexpr BOOST_FORCEINLINE std::enable_if_t<std::is_scalar_v<Scalar>> verify_one(Scalar const& pred)
    {
        if constexpr(std::is_integral_v<Scalar>)
        {
            if constexpr(std::is_same_v<Scalar, bool>)
            {
                if (!pred) throw std::logic_error{ "condition false" };
            }
            else if constexpr(std::is_signed_v<Scalar>) 
            {
                if (pred < 0)  throw std::out_of_range{ "negative value" };
            }            
        }
        else if constexpr(std::is_null_pointer_v<Scalar>)
            throw null_pointer_error{ "null pointer" };
        else if constexpr(std::is_pointer_v<Scalar>)
        {
            if (pred == nullptr)
                throw dangling_pointer_error{ "dangling pointer, pointer type: " + core::type_shortname<Scalar>() };
        }
        else throw std::invalid_argument{ "illegal parameter, type: "+ core::type_shortname<Scalar>() };
    }

    template<typename ...Types>
    constexpr BOOST_FORCEINLINE void verify(Types const& ...preds)
    {
        // TODO: sfinae for boolean convertible case
        (..., core::verify_one<Types>(preds));
    };
}
