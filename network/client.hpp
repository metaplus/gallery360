#pragma once

namespace net
{
    class client    // non-generic initial version
        : protected base::session_pool<boost::asio::ip::tcp, boost::asio::basic_stream_socket, std::unordered_map>
        , public std::enable_shared_from_this<client>
    {
    public:
        client() = delete;

        explicit client(std::shared_ptr<boost::asio::io_context> context)
            : session_pool(std::move(context))
            , resolver_(*io_context_ptr_)
            , client_strand_(*io_context_ptr_)
        {}

        client(const client&) = delete;

        client& operator=(const client&) = delete;

        using session_pool::session;

        struct stage {
            struct during_make_session : boost::noncopyable
            {
                explicit during_make_session(std::shared_ptr<client> self, std::promise<std::shared_ptr<session>> promise,
                    std::string_view host, std::string_view service)
                    : client_ptr(std::move(self))
                    , session_promise(std::move(promise))
                    , host(host)
                    , service(service)
                    , session_socket(*client_ptr->io_context_ptr_)
                {}
                
                std::shared_ptr<client> client_ptr;
                std::promise<std::shared_ptr<session>> session_promise;
                std::string_view host; 
                std::string_view service;
                socket session_socket;
            };
        };

        [[nodiscard]] std::future<std::shared_ptr<session>> establish_session(std::string_view host, std::string_view service)
        {
            std::promise<std::shared_ptr<session>> session_promise;
            auto session_future = session_promise.get_future();
            post(client_strand_, [=, promise = std::move(session_promise), self = shared_from_this()]() mutable
            {   
                resolve_requests_.emplace_back(std::move(self), std::move(promise), host, service);
                if (std::exchange(resolve_disposing_, true)) return;
                fmt::print("start dispose resolve\n");
                dispose_resolve(resolve_requests_.begin());
            });
            return session_future;
        }

    protected:
        void dispose_resolve(std::list<stage::during_make_session>::iterator stage_iter)
        {
            assert(resolve_disposing_);
            assert(client_strand_.running_in_this_thread());
            if (stage_iter == resolve_requests_.end())
            {
                resolve_disposing_ = false;
                return fmt::print("stop dispose resolve\n");
            }
            resolver_.async_resolve(stage_iter->host, stage_iter->service,
                bind_executor(client_strand_, [stage_iter, this](boost::system::error_code error, boost::asio::ip::tcp::resolver::results_type endpoints)
            {
                const auto guard = make_fault_guard(error, stage_iter->session_promise, "resolve failure");
                if (error) return;
                resolve_endpoints_.emplace_back(endpoints, stage_iter);
                if (!std::exchange(connect_disposing_, true))
                {
                    fmt::print("start dispose connect\n");
                    post(client_strand_, [this] { dispose_connect(); });
                }
                dispose_resolve(std::next(stage_iter));
            }));
        }

        void dispose_connect()
        {
            assert(connect_disposing_);
            assert(client_strand_.running_in_this_thread());
            if(resolve_endpoints_.empty())
            {
                connect_disposing_ = false;
                return fmt::print("stop dispose connect\n");
            }
            const auto[endpoints, stage_iter] = std::move(resolve_endpoints_.front());
            resolve_endpoints_.pop_front();
            async_connect(stage_iter->session_socket, endpoints, 
                bind_executor(client_strand_, [stage_iter, this](boost::system::error_code error, boost::asio::ip::tcp::endpoint endpoint)
            {
                const auto guard = make_fault_guard(error, stage_iter->session_promise, "connect failure");
                if (error) return;
                auto session_ptr = std::make_shared<session>(std::move(stage_iter->session_socket));
                stage_iter->client_ptr->add_session(session_ptr);
                stage_iter->session_promise.set_value(std::move(session_ptr));
                resolve_requests_.erase(stage_iter);    //  finish current stage 
                dispose_connect();
            }));
        }

    private:
        boost::asio::ip::tcp::resolver resolver_;
        std::list<stage::during_make_session> resolve_requests_;
        std::deque<std::pair<boost::asio::ip::tcp::resolver::results_type, decltype(resolve_requests_)::iterator>> resolve_endpoints_;
        mutable boost::asio::io_context::strand client_strand_;
        mutable bool resolve_disposing_ = false;
        mutable bool connect_disposing_ = false;
    };
}