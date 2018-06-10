#pragma once

namespace net
{
#ifdef NET_USE_LEGACY
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
#endif

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
                std::shared_ptr<boost::asio::io_context> const execution_;
                boost::asio::ip::tcp::resolver resolver_;
                std::list<boost::promise<boost::asio::ip::tcp::socket>> socket_list_;
                std::unordered_map<
                    std::pair<std::string_view, std::string_view>,
                    boost::asio::ip::tcp::resolver::results_type,
                    boost::hash<std::pair<std::string_view, std::string_view>>
                > endpoint_cache_;
               // std::promise<std::shared_ptr<boost::asio::ip::tcp::socket>> socket_promise_;
                boost::asio::io_context::strand mutable resolver_strand_;
                boost::asio::io_context::strand mutable socket_list_strand_;

            public:
                explicit connector(std::shared_ptr<boost::asio::io_context> ctx)
                    : execution_(std::move(ctx))
                    , resolver_(*execution_)
                    , resolver_strand_(*execution_)
                    , socket_list_strand_(*execution_)
                {}

                template<typename ApplicationProtocal, typename ...Types>
                std::shared_ptr<session<ApplicationProtocal, boost::asio::ip::tcp::socket>> establish_session(std::string_view host, std::string_view service, Types&& ...args)
                {
                    boost::promise<boost::asio::ip::tcp::socket> socket_promise;
                    auto socket_future = socket_promise.get_future();
                    boost::asio::post(socket_list_strand_, 
                        [socket_promise = std::move(socket_promise), this, self = shared_from_this()]() mutable 
                    {
                        socket_list_.push_front(std::move(socket_promise));
                        boost::asio::post(resolver_strand_,
                            [=, socket_iter = socket_list_.begin()]{ self->do_resolve(host,service,socket_iter); });
                    });
                    return std::make_shared<session<ApplicationProtocal, boost::asio::ip::tcp::socket>>(socket_future.get(), execution_, std::forward<Types>(args)...);
                }

            private:
                void do_resolve(std::string_view host, std::string_view service, decltype(socket_list_)::iterator socket_iter)
                {
                    assert(resolver_strand_.running_in_this_thread());
                    auto const endpoints_iter = endpoint_cache_.find(std::make_pair(host, service));
                    if (endpoints_iter != endpoint_cache_.end()) return do_connect(endpoints_iter->second, socket_iter);
                    resolver_.async_resolve(host, service, bind_executor(resolver_strand_,
                        [=, self = shared_from_this()](boost::system::error_code errc, boost::asio::ip::tcp::resolver::results_type endpoints)
                    {
                        if (errc)
                        {
                            socket_iter->set_exception(std::make_exception_ptr(std::runtime_error{ errc.message() }));
                            return fmt::print(std::cerr, "resolve errc {}, errmsg {}\n", errc, errc.message());
                        }
                        auto const[endpoints_iter, success] = endpoint_cache_.emplace(std::make_pair(host, service), std::move(endpoints));
                        assert(success);
                        auto const& endpoints_ref = endpoints_iter->second;
                        boost::asio::post(*execution_, [=, &endpoints_ref, self = shared_from_this()]{ do_connect(endpoints_ref,socket_iter); });
                    }));

                }

                void do_connect(decltype(endpoint_cache_)::mapped_type const& endpoints, decltype(socket_list_)::iterator socket_iter)
                {   // hash table iterator invalidates when rehashing happens
                    assert(execution_->get_executor().running_in_this_thread());
                    auto const socket = std::make_shared<boost::asio::ip::tcp::socket>(*execution_);
                    boost::asio::async_connect(*socket, endpoints, // bind_executor(resolver_strand_,
                        [=, self = shared_from_this()](boost::system::error_code errc, boost::asio::ip::tcp::endpoint endpoint)
                    {
                        if (errc)
                        {
                            socket_iter->set_exception(std::make_exception_ptr(std::runtime_error{ errc.message() }));
                            fmt::print(std::cerr, "accept errc {}, errmsg {}\n", errc, errc.message());
                        }
                        else socket_iter->set_value(std::move(*socket));
                        boost::asio::dispatch(socket_list_strand_, [=] { socket_list_.erase(socket_iter); });
                    });
                }
            };
        }
    }

    namespace client = v2::client;
}