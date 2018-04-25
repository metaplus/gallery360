#pragma once

namespace net::client
{
    class session_pool
    {
    public:
        session_pool() = delete;

        explicit session_pool(std::shared_ptr<boost::asio::io_context> context)
            : io_context_ptr_(std::move(context))
            , resolver_(*io_context_ptr_)
            , pool_strand_(*io_context_ptr_)
            , resolver_strand_(*io_context_ptr_)
        {}

        session_pool(const session_pool&) = delete;

        session_pool& operator=(const session_pool&) = delete;

    private:
        struct make_session_stage
        {
            make_session_stage() = delete;

            explicit make_session_stage(boost::asio::io_context& ioc)
                : socket(ioc)
            {}

            make_session_stage(make_session_stage&&) noexcept = default;

            make_session_stage& operator=(make_session_stage&&) noexcept = default;

            std::promise<std::weak_ptr<session<boost::asio::ip::tcp>>> session_promise;
            boost::asio::ip::tcp::socket socket;
        };

    public:
        [[nodiscard]] std::future<std::weak_ptr<session<boost::asio::ip::tcp>>>
            make_session(std::string_view host, std::string_view service)
        {
            auto stage = std::make_unique<make_session_stage>(*io_context_ptr_);
            auto make_session_future = stage->session_promise.get_future();
            //  make concurrency access to tcp::resolver thread-safe
            post(resolver_strand_, [=, stage = std::move(stage)]() mutable
            {
                resolver_.async_resolve(host, service,
                    std::bind(&session_pool::handle_resolve, this,
                        std::placeholders::_1, std::placeholders::_2, std::move(stage)));
            });
            return make_session_future;
        }

    private:
        struct callback_collection
        {   //  TODO: currently a dummy placeholder
            std::vector<std::shared_future<std::any>> dummy;
        };

        // struct handler  
        // {
        //     struct for_resolve {};
        //     struct for_connect {};
        // };

        std::unordered_map<
            std::shared_ptr<session<boost::asio::ip::tcp>>,
            callback_collection,
            session_element::dereference_hash,
            session_element::dereference_equal
            //core::dereference_delegate<std::hash<session<boost::asio::ip::tcp>>>,
            //core::dereference_delegate<std::equal_to<session<boost::asio::ip::tcp>>>
        > session_pool_;

        std::shared_ptr<boost::asio::io_context> io_context_ptr_;
        boost::asio::ip::tcp::resolver resolver_;
        boost::asio::io_context::strand pool_strand_;
        boost::asio::io_context::strand resolver_strand_;

        void handle_resolve(
            const boost::system::error_code& error,
            const boost::asio::ip::tcp::resolver::results_type& endpoints,
            std::unique_ptr<make_session_stage>& stage)
        {
            const auto guard = core::make_guard([&error, &stage]
            {
                if (!std::uncaught_exceptions() && !error) return;
                stage->session_promise.set_exception(
                    std::make_exception_ptr(std::runtime_error{ "endpoint resolvement failure" }));
                if (error) fmt::print(std::cerr, "error: {}\n", error.message());
            });
            if (error) return;
            auto& socket_ref = stage->socket;
            async_connect(socket_ref, endpoints,
                std::bind(&session_pool::handle_connect, this, std::placeholders::_1, std::move(stage)));
        }

        void handle_connect(
            const boost::system::error_code& error,
            std::unique_ptr<make_session_stage>& callback)
        {
            const auto guard = core::make_guard([&error, &callback]
            {
                if (!std::uncaught_exceptions() && !error) return;
                callback->session_promise.set_exception(
                    std::make_exception_ptr(std::runtime_error{ "socket connection failure" }));
                if (error) fmt::print(std::cerr, "error: {}\n", error.message());
            });
            if (error) return;
            auto session_ptr = std::make_shared<session<boost::asio::ip::tcp>>(std::move(callback->socket));
            std::weak_ptr<session<boost::asio::ip::tcp>> session_weak_ptr = session_ptr;
            session_pool_.emplace(std::move(session_ptr), callback_collection{});
            callback->session_promise.set_value(std::move(session_weak_ptr));
        }
    };
}