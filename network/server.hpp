#pragma once

namespace net
{
    namespace v1
    {
        template<typename Protocal>
        class server;

        template<>
        class server<boost::asio::ip::tcp>
            : protected base::session_pool<boost::asio::ip::tcp, boost::asio::basic_stream_socket, std::unordered_map>
            , protected core::noncopyable
            , public std::enable_shared_from_this<server<boost::asio::ip::tcp>>
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

    namespace server
    {
        template<typename Protocal, typename Socket = boost::asio::ip::tcp::socket>
        class session;

        template<>
        class session<http, boost::asio::ip::tcp::socket>
            : public std::enable_shared_from_this<session<http, boost::asio::ip::tcp::socket>>
        {
            using tcp = boost::asio::ip::tcp;
            using request_type = boost::beast::http::request<boost::beast::http::string_body>;
            using request_container = std::map<std::chrono::steady_clock::time_point, request_type>;

            const std::shared_ptr<boost::asio::io_context> execution_;
            const bool chunked_{ false };
            inline static const std::filesystem::path root_path{ "C:/Media" };
            tcp::socket socket_;
            request_container requests_;
            boost::beast::flat_buffer recvbuf_;
            mutable boost::asio::io_context::strand session_strand_;
            mutable boost::asio::io_context::strand process_strand_;
            mutable std::atomic<bool> active_{ false };
            mutable std::atomic<int64_t> request_consume_{ 0 };

        public:
            explicit session(tcp::socket sock, std::shared_ptr<boost::asio::io_context> ctx, bool chunked = false)
                : execution_(std::move(ctx))
                , chunked_(chunked)
                , socket_(std::move(sock))
                , session_strand_(*execution_)
                , process_strand_(*execution_)
            {
                assert(socket_.is_open());
                assert(core::address_same (*execution_, socket_.get_executor().context()));
                assert(is_directory(root_path));
                fmt::print(std::cout, "socket peer endpoint {}/{}\n", socket_.local_endpoint(), socket_.remote_endpoint());
                fmt::print(std::cout, "file root path {}\n", root_path);
            }

            session<http, tcp::socket>& run()
            {
                if (!std::atomic_exchange(&active_, true)) execute_recv();
                return *this;
            }

        private:
            void execute_recv()
            {
                if (!session_strand_.running_in_this_thread())
                    return boost::asio::dispatch(session_strand_, [this, self = shared_from_this()]() { execute_recv(); });
                const auto[iterator, success] = requests_.emplace(std::chrono::steady_clock::now(), request_container::mapped_type{});
                assert(success);
                auto& request = iterator->second;
                boost::beast::http::async_read(socket_, recvbuf_, request,
                    boost::asio::bind_executor(session_strand_, [&request, this, self = shared_from_this()](boost::system::error_code errc, std::size_t transfer_size)
                {
                    fmt::print(std::cout, "handle recv errc {}, transfer {}\n", errc, transfer_size);
                    if (errc)
                    {
                        fmt::print("errmsg {}\n", errc.message());
                        return socket_.shutdown(socket_.shutdown_send);
                    }
                    erase_consumed_request();
                    boost::asio::post(process_strand_, [&request, this, self]()
                    {
                        fmt::print(std::cout, "request head {}\n", request.base());
                        fmt::print(std::cout, "request body {}\n", request.body());
                        // TODO: external process routine towards request
                        std::atomic_fetch_add(&request_consume_, 1);
                    });
                    if (!chunked_) execute_send(request);
                    else assert(false);
                }));
            }

            void execute_send(request_type& request)
            {
                if (!session_strand_.running_in_this_thread())
                    return boost::asio::dispatch(session_strand_, [&request, this, self = shared_from_this()]() { execute_send(request); });
                const auto target_path = std::filesystem::path{ root_path }.concat(request.target().begin(), request.target().end());
                fmt::print(std::cout, "target file path {}, exists {}\n", target_path, exists(target_path));
                assert(exists(target_path));
                boost::beast::error_code file_errc;
                boost::beast::http::file_body::value_type response_body;
                response_body.open(target_path.generic_string().c_str(), boost::beast::file_mode::scan, file_errc);
                assert(!file_errc);
                auto response = std::make_unique<boost::beast::http::response<boost::beast::http::file_body>>(
                    boost::beast::http::status::ok, request.version(), std::move(response_body));
                response->set(boost::beast::http::field::server, "METAPLUS");
                // response->set(boost::beast::http::field::content_type, "video/mp4");
                response->content_length(response_body.size());
                response->keep_alive(request.keep_alive());
                auto& response_ref = *response;
                fmt::print(std::cout, "response head {}\n", response_ref.base());
                boost::beast::http::async_write(socket_, response_ref,
                    boost::asio::bind_executor(session_strand_,
                        [response = std::move(response), this, self = shared_from_this()](boost::system::error_code errc, std::size_t transfer_size)
                {
                    fmt::print(std::cout, "handle send errc {}, last {}, transfer {}\n", errc, response->need_eof(), transfer_size);
                    if (errc || response->need_eof())
                    {
                        if (errc) fmt::print("errmsg {}\n", errc.message());
                        return socket_.shutdown(socket_.shutdown_send);
                    }
                    execute_recv();
                }));
            }

            int64_t erase_consumed_request()
            {
                const auto consume_size = std::atomic_exchange(&request_consume_, 0);
                if (consume_size > 0) requests_.erase(requests_.begin(), std::next(requests_.begin(), consume_size));
                return consume_size;
            }
        };

        template<typename Protocal>
        class acceptor;

        template<>
        class acceptor<boost::asio::ip::tcp>
            : public std::enable_shared_from_this<acceptor<boost::asio::ip::tcp>>
        {
            using tcp = boost::asio::ip::tcp;

            const std::shared_ptr<boost::asio::io_context> execution_;
            tcp::acceptor acceptor_;
            tbb::concurrent_bounded_queue<std::shared_ptr<tcp::socket>> sockets_;
            mutable boost::asio::io_context::strand acceptor_strand_;
            mutable std::atomic<bool> active_{ false };

        public:
            acceptor(tcp::endpoint endpoint, std::shared_ptr<boost::asio::io_context> ctx, bool reuse_addr = false)
                : execution_(std::move(ctx))
                , acceptor_(*execution_, endpoint, reuse_addr)
                , acceptor_strand_(*execution_)
            {
                assert(acceptor_.is_open());
                fmt::print("acceptor listen address {}, port {}\n", endpoint.address(), listen_port());
            }

            uint16_t listen_port() const
            {
                return acceptor_.local_endpoint().port();
            }

            acceptor<boost::asio::ip::tcp>& run()
            {
                if (!std::atomic_exchange(&active_, true)) execute_accept();
                return *this;
            }

            template<typename ApplicationProtocal, typename ...Types>
            std::shared_ptr<session<ApplicationProtocal, tcp::socket>> listen_session(Types&& ...args)
            {
                run();
                std::shared_ptr<tcp::socket> socket;
                sockets_.pop(socket);
                return std::make_shared<session<ApplicationProtocal, tcp::socket>>(std::move(*socket), execution_, std::forward<Types>(args)...);
            }

        private:
            void execute_accept()
            {
                boost::asio::dispatch(acceptor_strand_, [this, self = shared_from_this()]() mutable
                {
                    acceptor_.async_accept([this, self = std::move(self)](boost::system::error_code errc, tcp::socket socket)
                    {
                        if (errc) return fmt::print(std::cerr, "handle accept error: {}\n", errc);
                        boost::asio::post(*execution_,
                            [socket = std::make_shared<tcp::socket>(std::move(socket)), this, self]() mutable { sockets_.push(std::move(socket)); });
                        execute_accept();
                    });
                });
            }
        };

        template class acceptor<boost::asio::ip::tcp>;

        namespace test
        {
            using test_session_type = decltype(std::declval<acceptor<boost::asio::ip::tcp>&>().listen_session<http>());
        }
    }
}