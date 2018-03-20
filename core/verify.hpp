#pragma once

namespace core
{
    template<typename Scalar>
    constexpr std::enable_if_t<std::is_scalar_v<Scalar>> verify_one(Scalar pred)
    {
        if constexpr(std::is_integral_v<Scalar>)
        {
            if constexpr(std::is_same_v<Scalar, bool>)
            {
                if (!pred)
                    throw std::logic_error{ "condition false" };
            }
            else if (pred < 0)
                throw std::out_of_range{ "negative value" };
        }
        else if constexpr(std::is_null_pointer_v<Scalar>)
            throw core::null_pointer_error{ "null pointer" };
        else if constexpr(std::is_pointer_v<Scalar>)
        {
            if (pred == nullptr)
                throw core::dangling_pointer_error{ "dangling pointer, pointer type: " + core::type_shortname<Scalar>() };
        }
        else throw std::invalid_argument{ "illegal parameter, type: "+ core::type_shortname<Scalar>() };
    }

    template<typename ...Types>
    constexpr void verify(Types ...preds)
    {
        /* TODO: reserved legacy
        class exception_proxy
        {
        public:
            explicit exception_proxy(const std::exception_ptr& e) :describe_{}, error_{ e } { }
            void message(std::string&& msg) noexcept { describe_ = std::move(msg); }
            void operator[](std::string&& msg) noexcept { message(std::move(msg)); }
            ~exception_proxy() noexcept(false)
            {
                if (error_)
                {
                    auto&& thread_hash_id{ boost::lexical_cast<std::string>(std::this_thread::get_id()) };
                    try { std::rethrow_exception(error_); }
                    catch (...) { std::throw_with_nested(std::runtime_error{ "error@" + std::move(thread_hash_id) + " message@" + describe_ }); }
                }
            }
        private:
            std::string describe_;
            std::exception_ptr error_;s
        };
        std::exception_ptr exception_handler{ nullptr };
        */
        try
        {   // TODO sfinae for boolean convertible case
            (..., core::verify_one<Types>(preds));
        }
        catch (...)
        {
            //std::throw_with_nested(fmt::format("thread@{} ",
            //    boost::lexical_cast<std::string>(std::this_thread::get_id())));
            //exception_handler = std::current_exception();
            std::cerr << "thread@" << std::this_thread::get_id() << ' ';
            throw;
        }
        //return exception_proxy{ exception_handler };
    };
}