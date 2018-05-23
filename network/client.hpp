#pragma once

namespace net
{
    namespace v1
    {
        class client    // non-generic initial version
            : protected base::session_pool<boost::asio::ip::tcp, boost::asio::basic_stream_socket, std::unordered_map>
            , protected core::noncopyable
            , public std::enable_shared_from_this<client>
        {
        public:
            client() = delete;

            explicit client(std::shared_ptr<boost::asio::io_context> context)
                : session_pool(std::move(context))
                , resolver_(*io_context_ptr_)
                , client_strand_(*io_context_ptr_)
            {}

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
                if (resolve_endpoints_.empty())
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

    namespace v2
    {
        namespace client
        {
            template<typename Protocal, typename Socket = boost::asio::ip::tcp::socket>
            class session;

            template<>
            class session<protocal::http, boost::asio::ip::tcp::socket>
                : public std::enable_shared_from_this<session<protocal::http, boost::asio::ip::tcp::socket>>
            {
                const std::shared_ptr<boost::asio::io_context> execution_;
                boost::asio::ip::tcp::socket socket_;
                boost::beast::flat_buffer recvbuf_;
                mutable boost::asio::io_context::strand strand_;
                mutable std::atomic<bool> active_{ false };

                session(boost::asio::ip::tcp::socket sock, std::shared_ptr<boost::asio::io_context> ctx)
                    : execution_(std::move(ctx))
                    , socket_(std::move(sock))
                    , strand_(*execution_)
                {
                    assert(socket_.is_open());
                    assert(core::address_same(*execution_, socket_.get_executor().context()));
                    fmt::print(std::cout, "socket peer endpoint {}/{}\n", socket_.local_endpoint(), socket_.remote_endpoint());
                }

            };

            template<typename Protocal>
            class connector;

            template<>
            class connector<boost::asio::ip::tcp>
                : public std::enable_shared_from_this<connector<boost::asio::ip::tcp>>
            {
                const std::shared_ptr<boost::asio::io_context> execution_;
                boost::asio::ip::tcp::resolver resolver_;
                std::unordered_map<std::pair<std::string_view, std::string_view>, boost::asio::ip::tcp::resolver::results_type, core::hash<void>> endpoint_cache_;
                std::promise<std::shared_ptr<boost::asio::ip::tcp::socket>> socket_promise_;
                mutable boost::asio::io_context::strand connector_strand_;
                mutable std::atomic<bool> active_{ false };

            public:
                explicit connector(std::shared_ptr<boost::asio::io_context> ctx)
                    : execution_(std::move(ctx))
                    , resolver_(*execution_)
                    , connector_strand_(*execution_)
                {}

                template<typename ApplicationProtocal, typename ...Types>
                std::shared_ptr<session<ApplicationProtocal, boost::asio::ip::tcp::socket>> establish_session(std::string_view host, std::string_view service, Types&& ...args)
                {
                    // run();
                    assert(!std::atomic_exchange(&active_, true));
                    auto socket_future = (socket_promise_ = {}).get_future();
                    execute_resolve(host, service);
                    return std::make_shared<session<ApplicationProtocal, boost::asio::ip::tcp::socket>>(socket_future.get(), execution_, std::forward<Types>(args)...);
                }

            private:
                void execute_resolve(std::string_view host, std::string_view service)
                {
                    if (!connector_strand_.running_in_this_thread())
                        return dispatch(connector_strand_, [host, service, this, self = shared_from_this()]{ execute_resolve(host,service); });
                    const auto endpoint_iter = endpoint_cache_.find(std::make_pair(host, service));
                    if (endpoint_iter != endpoint_cache_.end()) return execute_connect(endpoint_iter->second);
                    resolver_.async_resolve(host, service, bind_executor(connector_strand_,
                        [host, service, this, self = shared_from_this()](boost::system::error_code errc, boost::asio::ip::tcp::resolver::results_type endpoints)
                    {
                        if (errc)
                        {
                            socket_promise_.set_exception(std::make_exception_ptr(std::runtime_error{ errc.message() }));
                            return fmt::print(std::cerr, "resolve errc {}, errmsg {}\n", errc, errc.message());
                        }
                        const auto[iterator, success] = endpoint_cache_.emplace(std::make_pair(host, service), std::move(endpoints));
                        assert(success);
                        execute_connect(iterator->second);
                    }));

                }

                void execute_connect(const boost::asio::ip::tcp::resolver::results_type& endpoints)
                {
                    if (!connector_strand_.running_in_this_thread())
                        return dispatch(connector_strand_, [&endpoints, this, self = shared_from_this()]{ execute_connect(endpoints); });
                    const auto socket = std::make_shared<boost::asio::ip::tcp::socket>(*execution_);
                    async_connect(*socket, endpoints, bind_executor(connector_strand_,
                        [socket, this, self = shared_from_this()](boost::system::error_code errc, boost::asio::ip::tcp::endpoint endpoint)
                    {
                        assert(std::atomic_exchange(&active_, false));
                        if (errc)
                        {
                            socket_promise_.set_exception(std::make_exception_ptr(std::runtime_error{ errc.message() }));
                            return fmt::print(std::cerr, "accept errc {}, errmsg {}\n", errc, errc.message());
                        }
                        socket_promise_.set_value(socket);
                        socket_promise_ = {};
                    }));
                }
            };
        }
    }

    namespace client = v2::client;
}