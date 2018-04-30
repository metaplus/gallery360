#pragma once

namespace net
{
    class client : protected base::session_pool<boost::asio::ip::tcp, boost::asio::basic_stream_socket, std::unordered_map, std::shared_ptr>
    {
    public:
        client() = delete;

        explicit client(std::shared_ptr<boost::asio::io_context> context)
            : session_pool(std::move(context))
            , resolver_(*io_context_ptr_)
            , resolver_strand_(*io_context_ptr_)
        {}

        client(const client&) = delete;

        client& operator=(const client&) = delete;

        struct stage {
            struct during_make_session : boost::noncopyable
            {
                explicit during_make_session(boost::asio::io_context& context)
                    : session_socket(context)
                {}

                std::promise<std::weak_ptr<session>> session_promise;
                socket session_socket;
            };
        };

        [[nodiscard]] std::future<std::weak_ptr<session>> make_session(std::string_view host, std::string_view service)
        {
            auto stage = std::make_unique<stage::during_make_session>(*io_context_ptr_);
            auto session_future = stage->session_promise.get_future();
            post(resolver_strand_, [=, stage = std::move(stage)]() mutable
            {   // TODO: adapt std::bind to lambda-expression
                resolver_.async_resolve(host, service,
                    std::bind(&client::handle_resolve, this,
                        std::placeholders::_1, std::placeholders::_2, std::move(stage)));
            });
            return session_future;
        }

    private:
        boost::asio::ip::tcp::resolver resolver_;
        boost::asio::io_context::strand resolver_strand_;

        void handle_resolve(
            const boost::system::error_code& error,
            const boost::asio::ip::tcp::resolver::results_type& endpoints,
            std::unique_ptr<stage::during_make_session>& stage)
        {
            const auto guard = core::make_guard([&error, &stage]
            {
                if (!std::uncaught_exceptions() && !error) return;
                stage->session_promise.set_exception(
                    std::make_exception_ptr(std::runtime_error{ "endpoint resolvement failure" }));
                if (error) fmt::print(std::cerr, "error: {}\n", error.message());
            });
            if (error) return;
            auto& socket_ref = stage->session_socket;
            async_connect(socket_ref, endpoints,
                std::bind(&client::handle_connect, this, std::placeholders::_1, std::move(stage)));
        }

        void handle_connect(
            const boost::system::error_code& error,
            std::unique_ptr<stage::during_make_session>& stage)
        {
            const auto guard = core::make_guard([&error, &stage]
            {
                if (!std::uncaught_exceptions() && !error) return;
                stage->session_promise.set_exception(
                    std::make_exception_ptr(std::runtime_error{ "socket connection failure" }));
                if (error) fmt::print(std::cerr, "error: {}\n", error.message());
            });
            if (error) return;
            auto session_ptr = std::make_shared<session>(std::move(stage->session_socket));
            std::weak_ptr<session> session_weak_ptr = session_ptr;
            session_pool_.emplace(std::move(session_ptr), callback_container{});
            stage->session_promise.set_value(std::move(session_weak_ptr));
        }
    };
}