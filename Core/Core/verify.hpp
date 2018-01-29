#pragma once
namespace core
{
    inline auto verify_one = [](auto pred) constexpr ->void
    {
        using type = decltype(pred);
        static_assert(std::is_scalar_v<type>, "claim predicate confoming to scalar");
        if (std::is_same_v<type, bool> && !pred) { throw std::runtime_error{ "error@condition false" }; }
        if (std::is_arithmetic_v<type> && pred < 0) { throw std::runtime_error{ "error@negative value" }; }
        if (std::is_null_pointer_v<type>) { throw std::runtime_error{ "error@null pointer" }; }
        if (std::is_pointer_v<type> && !pred) { throw std::runtime_error{ "error@null pointer" }; }
    };
    inline auto verify = [](auto ...preds) constexpr ->void
    {
        /*
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
            std::exception_ptr error_;
        };
        std::exception_ptr exception_handler{ nullptr };
        */
        try
        {
            //(verify_one(preds),...);
            (..., verify_one(preds));
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
