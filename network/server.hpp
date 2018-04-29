#pragma once

namespace net
{
    class server
        : protected base::session_pool<boost::asio::ip::tcp, boost::asio::basic_stream_socket, std::unordered_map, std::shared_ptr>
    {
    public:
        server() = delete;

        explicit server(std::shared_ptr<boost::asio::io_context> context)
            : session_pool(std::move(context))
            , acceptor_(*io_context_ptr_)
            , acceptor_strand_(*io_context_ptr_)
        {}

        server(std::shared_ptr<boost::asio::io_context> context, boost::asio::ip::tcp::endpoint endpoint)
            : session_pool(std::move(context))
            , acceptor_(*io_context_ptr_, endpoint, false)  //  reuse_addr = false, rebind port if conflicted
            , acceptor_strand_(*io_context_ptr_)
        {}

        server(const server&) = delete;

        server& operator=(const server&) = delete;

        void bind_endpoint(boost::asio::ip::tcp::endpoint endpoint)
        {
            assert(!acceptor_.is_open());
            if (acceptor_.is_open()) acceptor_.close();
            acceptor_.open(endpoint.protocol());
            acceptor_.bind(endpoint);
        }

        [[nodiscard]] std::future<std::shared_ptr<session>> wait_session()
        {
            auto stage = std::make_unique<stage::during_wait_session>();
            auto session_future = stage->session_promise.get_future();
            post(acceptor_strand_, [this, stage = std::move(stage)]() mutable
            {
                acceptor_.async_accept([this, stage = std::move(stage)](boost::system::error_code error, socket socket) 
                {
                    const auto guard = core::make_guard([&error, &stage]
                    {
                        if (!std::uncaught_exceptions() && !error) return;
                        stage->session_promise.set_exception(
                            std::make_exception_ptr(std::runtime_error{ "socket waiting failure" }));
                        if (error) fmt::print(std::cerr, "error: {}\n", error.message());
                    });
                    if (error) return;
                    auto session_ptr = std::make_shared<session>(std::move(socket));
                    session_pool_.emplace(session_ptr, callback_container{});
                    stage->session_promise.set_value(std::move(session_ptr));
                });
            });
            return session_future;
        }

        uint16_t listen_port() const
        {
            return acceptor_.local_endpoint().port();
        }

        bool is_non_blocking() const
        {            
            return acceptor_.non_blocking();
        }

        void is_non_blocking(bool mode)
        {
            acceptor_.non_blocking(mode);
        }

        bool is_open() const
        {
            return acceptor_.is_open();
        }

        explicit operator bool() const
        {
            return is_open();
        }

    private:
        boost::asio::ip::tcp::acceptor acceptor_;
        boost::asio::io_context::strand acceptor_strand_;
    };
}