#pragma once
namespace core
{
    template<typename Scalar>
    constexpr auto verify_one(Scalar pred) ->std::enable_if_t<std::is_scalar_v<Scalar>>
    {
        if constexpr(std::is_integral_v<Scalar>) {
            if constexpr(std::is_same_v<Scalar, bool>) {
                if (!pred) throw std::logic_error{ "verify@condition false" };
            }
            else if (pred < 0) throw std::logic_error{ "verify@negative value" };
        }
        if constexpr(std::is_null_pointer_v<Scalar>)
            throw std::runtime_error{ "verify@null pointer" };
        if constexpr(std::is_pointer_v<Scalar>) {
            if (pred == nullptr) throw std::runtime_error{ "verify@allcate nothing" };
        }
        else throw std::invalid_argument{ "verify@illegal parameter" };
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
                    auto&& thread_id{ boost::lexical_cast<std::string>(std::this_thread::get_id()) };
                    try { std::rethrow_exception(error_); }
                    catch (...) { std::throw_with_nested(std::runtime_error{ "error@" + std::move(thread_id) + " message@" + describe_ }); }
                }
            }
        private:
            std::string describe_;
            std::exception_ptr error_;s
        };
        std::exception_ptr exception_handler{ nullptr };
        */
        try
        {
            (..., core::verify_one<Types>(preds));
        }
        catch (...)
        {
            //std::throw_with_nested(fmt::format("thread@{} ",
            //    boost::lexical_cast<std::string>(std::this_thread::get_id())));
            //exception_handler = std::current_exception();
            std::cerr << "thread@" << std::this_thread::get_id() << ' ';
            //boost::lexical_cast<std::string>(std::this_thread::get_id()));
            throw;
        }
        //return exception_proxy{ exception_handler };
    };
    int inspect_exception(const std::exception& e);
}
inline int core::inspect_exception(const std::exception & e)
{
    std::cerr << e.what() << ' ';
    try {
        std::rethrow_if_nested(e);
    }
    catch (const std::exception& another) {
        return core::inspect_exception(another);
    }
    catch (...) {
        std::cerr << "nonstandard exception";
    }
    std::cerr << std::endl;
    return boost::exit_failure;
}
