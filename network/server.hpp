#pragma once

namespace net
{
    class server     // non-generic initial version
        : protected base::session_pool<boost::asio::ip::tcp, boost::asio::basic_stream_socket, std::unordered_map>
        , public std::enable_shared_from_this<server>
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
        {
            fmt::print("listen on port: {}\n", listen_port());
        }

        server(const server&) = delete;

        server& operator=(const server&) = delete;

        using session_pool::session;

        struct stage {
            struct during_wait_session : boost::noncopyable
            {
                during_wait_session(std::shared_ptr<server> self, std::promise<std::shared_ptr<session>> promise)
                    : server_ptr(std::move(self))
                    , session_promise(std::move(promise))
                    , endpoint(std::nullopt)
                {}

                during_wait_session(std::shared_ptr<server> self, std::promise<std::shared_ptr<session>> promise, protocal::endpoint endpoint)
                    : server_ptr(std::move(self))
                    , session_promise(std::move(promise))
                    , endpoint(endpoint)
                {}

                std::shared_ptr<server> server_ptr;
                std::promise<std::shared_ptr<session>> session_promise;
                std::optional<protocal::endpoint> endpoint;
            };
        };

        void bind_endpoint(protocal::endpoint endpoint)
        {
            assert(!acceptor_.is_open());
            if (acceptor_.is_open()) acceptor_.close();
            acceptor_.open(endpoint.protocol());
            acceptor_.bind(endpoint);
        }

        [[nodiscard]] std::future<std::shared_ptr<session>> accept_session()
        {
            std::promise<std::shared_ptr<session>> session_promise;
            auto session_future = session_promise.get_future();
            post(acceptor_strand_, [promise = std::move(session_promise), this, self = shared_from_this()]() mutable
            {                
                accept_requests_.emplace_back(std::move(self), std::move(promise));
                if (std::exchange(accept_disposing_, true)) return;
                fmt::print("start dispose accept\n");
                dispose_accept(accept_requests_.begin());
            });
            return session_future;
        }

        [[nodiscard]] std::future<std::shared_ptr<session>> accept_session(protocal::endpoint endpoint)
        {
            std::promise<std::shared_ptr<session>> session_promise;
            auto session_future = session_promise.get_future();
            post(acceptor_strand_, [endpoint, promise = std::move(session_promise), this, self = shared_from_this()]() mutable
            {
                accept_requests_.emplace_back(std::move(self), std::move(promise), endpoint);
                if (std::exchange(accept_disposing_, true)) return;
                fmt::print("start dispose accept\n");
                dispose_accept(accept_requests_.begin());
            });
            return session_future;
        }

        uint16_t listen_port() const
        {
            return acceptor_.local_endpoint().port();
        }

        bool is_open() const
        {
            return acceptor_.is_open();
        }

        explicit operator bool() const
        {
            return is_open();
        }

    protected:
        void dispose_accept(std::list<stage::during_wait_session>::iterator stage_iter)
        {
            assert(accept_disposing_);
            assert(acceptor_strand_.running_in_this_thread());
            if (stage_iter == accept_requests_.end())
            {
                accept_disposing_ = false;
                return fmt::print("stop dispose accept\n");
            }
            auto serial_handler = bind_executor(acceptor_strand_, [stage_iter, this](boost::system::error_code error, socket socket)
            {
                fmt::print("sock address {}/{}\n", socket.local_endpoint(), socket.remote_endpoint());
                const auto guard = make_fault_guard(error, stage_iter->session_promise, "socket accept failure");
                if (error) return;
                auto session_ptr = std::make_shared<session>(std::move(socket));
                stage_iter->server_ptr->add_session(session_ptr);
                stage_iter->session_promise.set_value(std::move(session_ptr));
                const auto next_iter = accept_requests_.erase(stage_iter);  //  finish current stage 
                dispose_accept(next_iter);
            });
            if (stage_iter->endpoint.has_value())
                return acceptor_.async_accept(stage_iter->endpoint.value(), std::move(serial_handler));
            acceptor_.async_accept(std::move(serial_handler));
        }

    private:
        boost::asio::ip::tcp::acceptor acceptor_;
        std::list<stage::during_wait_session> accept_requests_;
        mutable boost::asio::io_context::strand acceptor_strand_;
        mutable bool accept_disposing_ = false;
    };
}