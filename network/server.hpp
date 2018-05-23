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
        class session<protocal::http, boost::asio::ip::tcp::socket>
            : public std::enable_shared_from_this<session<protocal::http, boost::asio::ip::tcp::socket>>
        {
            using request_type = boost::beast::http::request<boost::beast::http::string_body>;
            using request_container = std::map<std::chrono::steady_clock::time_point, request_type>;

            const std::shared_ptr<boost::asio::io_context> execution_;
            const bool chunked_{ false };
            const std::filesystem::path root_path_;
            inline static const std::filesystem::path default_root_path{ "C:/Media" };
            boost::asio::ip::tcp::socket socket_;
            request_container requests_;
            boost::beast::flat_buffer recvbuf_;
            mutable boost::asio::io_context::strand session_strand_;
            mutable boost::asio::io_context::strand process_strand_;
            mutable std::atomic<bool> active_{ false };
            mutable std::atomic<size_t> request_consume_{ 0 };

        public:
            session(boost::asio::ip::tcp::socket sock, std::shared_ptr<boost::asio::io_context> ctx, std::filesystem::path root)
                : execution_(std::move(ctx))
                , root_path_(std::move(root))
                , socket_(std::move(sock))
                , session_strand_(*execution_)
                , process_strand_(*execution_)
            {
                assert(socket_.is_open());
                assert(core::address_same(*execution_, socket_.get_executor().context()));
                assert(is_directory(default_root_path));
                fmt::print(std::cout, "socket peer endpoint {}/{}\n", socket_.local_endpoint(), socket_.remote_endpoint());
                fmt::print(std::cout, "file root path {}\n", default_root_path);
            }

            session(boost::asio::ip::tcp::socket sock, std::shared_ptr<boost::asio::io_context> ctx)
                : session(std::move(sock), std::move(ctx), default_root_path)
            {}

            session(boost::asio::ip::tcp::socket sock, std::shared_ptr<boost::asio::io_context> ctx, std::filesystem::path root, use_chunk_t)
                : session(std::move(sock), std::move(ctx), std::move(root))
            {
                const_cast<bool&>(chunked_) = true;
            }

            session(boost::asio::ip::tcp::socket sock, std::shared_ptr<boost::asio::io_context> ctx, use_chunk_t)
                : session(std::move(sock), std::move(ctx))
            {
                const_cast<bool&>(chunked_) = true;
            }

            session<protocal::http, boost::asio::ip::tcp::socket>& run()
            {
                if (!std::atomic_exchange(&active_, true)) execute_recv();
                return *this;
            }

        private:
            void execute_recv()
            {
                if (!session_strand_.running_in_this_thread())
                    return dispatch(session_strand_, [this, self = shared_from_this()]{ execute_recv(); });
                erase_consumed_request();
                auto& request = insert_empty_request()->second;
                fmt::print("wait incoming request, recvbuf size {}\n", recvbuf_.size());
                boost::beast::http::async_read(socket_, recvbuf_, request, bind_executor(session_strand_,
                    [&request, this, self = shared_from_this()](boost::system::error_code errc, std::size_t transfer_size)
                {
                    fmt::print(std::cout, "handle recv errc {}, transfer {}\n", errc, transfer_size);
                    if (errc) close_socket(errc, boost::asio::socket_base::shutdown_both);
                    post(process_strand_, [&request, this, self]()
                    {
                        fmt::print(std::cout, "request head {}\n", request.base());
                        fmt::print(std::cout, "request body {}\n", request.body());
                        // TODO: external process routine towards request
                        std::atomic_fetch_add(&request_consume_, 1);
                    });
                    if (chunked_) execute_send_header(request, use_chunk);
                    else execute_send(request);
                }));
            }

            void execute_send(request_type& request)
            {
                namespace http = boost::beast::http;
                if (!session_strand_.running_in_this_thread())
                    return dispatch(session_strand_, [&request, this, self = shared_from_this()]{ execute_send(request); });
                const auto target_path = concat_target_path(request.target());
                fmt::print(std::cout, "target file path {}, exists {}\n", target_path, exists(target_path));
                assert(exists(target_path));
                boost::system::error_code file_errc;
                http::file_body::value_type response_body;
                response_body.open(target_path.generic_string().c_str(), boost::beast::file_mode::scan, file_errc);
                assert(!file_errc);
                auto response = std::make_unique<http::response<http::file_body>>(http::status::ok, request.version(), std::move(response_body));
                response->set(http::field::server, "METAPLUS");
                // response->set(http::field::content_type, "video/mp4");
                response->content_length(response_body.size());
                response->keep_alive(request.keep_alive());
                auto& response_ref = *response;
                fmt::print(std::cout, "response head {}\n", response_ref.base());
                http::async_write(socket_, response_ref, bind_executor(session_strand_,
                    [response = std::move(response), this, self = shared_from_this()](boost::system::error_code errc, size_t transfer_size)
                {
                    fmt::print(std::cout, "handle send errc {}, last {}, transfer {}\n", errc, response->need_eof(), transfer_size);
                    if (errc || response->need_eof()) return close_socket(errc, boost::asio::socket_base::shutdown_send);
                    execute_recv();
                }));
            }

            struct chunk_read_context : core::noncopyable
            {
                using chunk_data = std::vector<char>;
                using chunk_type = std::shared_ptr<chunk_data>;

                inline static constexpr size_t default_chunk_size{ 8192 * 16 };

                std::filesystem::path file_path;
                tbb::concurrent_bounded_queue<chunk_type> chunk_queue;
                std::thread chunk_reader;

                chunk_read_context() = delete;

                explicit chunk_read_context(std::filesystem::path path)
                    : file_path(std::move(path))
                    , chunk_reader([this]
                    {
                        boost::beast::file file;
                        boost::system::error_code errc;
                        file.open(file_path.generic_string().c_str(), boost::beast::file_mode::scan, errc);
                        chunk_queue.set_capacity(1024);
                        assert(!errc && file.is_open());
                        size_t read_size{ 0 };
                        while (!errc)
                        {
                            const auto chunk = std::make_shared<chunk_data>(default_chunk_size);
                            const auto chunk_size = file.read(chunk->data(), chunk->size(), errc);
                            if (chunk_size < chunk->size()) chunk->resize(chunk_size);
                            chunk_queue.push(std::move(chunk));
                            // if (read_size < default_chunk_size)
                            //     fmt::print(std::cerr, "detect small chunk size {}, errc {}, errmsg {}\n", chunk_size, errc, errc.message());
                            read_size += chunk_size;
                        }
                        chunk_queue.emplace();
                    })
                {}

                explicit chunk_read_context(std::string_view path)
                    : chunk_read_context(std::filesystem::path{ path })
                {}

                std::shared_ptr<std::vector<char>> get_chunk()
                {
                    std::shared_ptr<std::vector<char>> chunk;
                    chunk_queue.pop(chunk);
                    return chunk;
                }

                ~chunk_read_context() { if (chunk_reader.joinable()) chunk_reader.join(); }
            };

            void execute_send_header(request_type& request, use_chunk_t)
            {
                namespace http = boost::beast::http;
                if (!session_strand_.running_in_this_thread())
                    return dispatch(session_strand_, [&request, this, self = shared_from_this()]{ execute_send_header(request,use_chunk); });
                auto response = std::make_shared<http::response<http::empty_body>>(http::status::ok, request.version());
                auto serializer = std::make_shared<http::response_serializer<http::empty_body>>(*response);
                auto& serializer_ref = *serializer;
                response->set(http::field::server, "METAPLUS");
                response->chunked(true);
                const auto read_context = std::make_shared<chunk_read_context>(concat_target_path(request.target()));
                http::async_write_header(socket_, serializer_ref, bind_executor(session_strand_,
                    [response, serializer, read_context, this, self = shared_from_this()](boost::system::error_code errc, size_t transfer_size)
                {
                    fmt::print("handle send header errc {}, transfer {}\n", errc, transfer_size);
                    if (errc) return close_socket(errc, boost::asio::socket_base::shutdown_send);
                    execute_send_chunk(read_context);
                }));
            }

            void execute_send_chunk(std::shared_ptr<chunk_read_context> read_context)
            {
                namespace http = boost::beast::http;
                if (!session_strand_.running_in_this_thread())
                    return dispatch(session_strand_, [read_context, this, self = shared_from_this()]{ execute_send_chunk(read_context); });
                const auto chunk = read_context->get_chunk();
                if (!chunk || chunk->empty())
                {
                    [[maybe_unused]] const auto write_size = boost::asio::write(socket_, http::make_chunk_last());
                    fmt::print("last chunk written, transfer size {}, ptr {}, empty {}\n", write_size, chunk != nullptr, chunk != nullptr && chunk->empty());
                    read_context = nullptr;
                    std::atomic_store(&active_, false);
                    return socket_.shutdown(boost::asio::socket_base::shutdown_both);
                    // return execute_recv();
                }
                auto& chunk_ref = *chunk;
                boost::asio::async_write(socket_, http::make_chunk(boost::asio::buffer(chunk_ref)), bind_executor(session_strand_,
                    [chunk, read_context, this, self = shared_from_this()](boost::system::error_code errc, size_t transfer_size)
                {
                    fmt::print(std::cout, "handle send errc {}, transfer {}\n", errc, transfer_size);
                    if (errc) return close_socket(errc, boost::asio::socket_base::shutdown_send);
                    execute_send_chunk(read_context);
                }));
            }

            std::filesystem::path concat_target_path(boost::beast::string_view request_target) const
            {
                return std::filesystem::path{ root_path_ }.concat(request_target.begin(), request_target.end());
            }

            request_container::iterator insert_empty_request()
            {
                const auto[iterator, success] = requests_.emplace(std::chrono::steady_clock::now(), request_container::mapped_type{});
                assert(success);
                return iterator;
            }

            size_t erase_consumed_request()
            {
                const auto consume_size = std::atomic_exchange(&request_consume_, 0);
                if (consume_size > 0) requests_.erase(requests_.begin(), std::next(requests_.begin(), consume_size));
                return consume_size;
            }

            void close_socket(boost::system::error_code errc, boost::asio::socket_base::shutdown_type operation)
            {
                fmt::print(std::cerr, "session errc {}, errmsg {}\n", errc, errc.message());
                socket_.shutdown(operation);
                erase_consumed_request();
            }
        };

        template<typename Protocal>
        class acceptor;

        template<>
        class acceptor<boost::asio::ip::tcp>
            : public std::enable_shared_from_this<acceptor<boost::asio::ip::tcp>>
        {
            const std::shared_ptr<boost::asio::io_context> execution_;
            boost::asio::ip::tcp::acceptor acceptor_;
            tbb::concurrent_bounded_queue<std::shared_ptr<boost::asio::ip::tcp::socket>> sockets_;
            mutable boost::asio::io_context::strand acceptor_strand_;
            mutable std::atomic<bool> active_{ false };

        public:
            acceptor(boost::asio::ip::tcp::endpoint endpoint, std::shared_ptr<boost::asio::io_context> ctx, bool reuse_addr = false)
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
            std::shared_ptr<session<ApplicationProtocal, boost::asio::ip::tcp::socket>> listen_session(Types&& ...args)
            {
                run();
                std::shared_ptr<boost::asio::ip::tcp::socket> socket;
                sockets_.pop(socket);
                return std::make_shared<session<ApplicationProtocal, boost::asio::ip::tcp::socket>>(std::move(*socket), execution_, std::forward<Types>(args)...);
            }

        private:
            void execute_accept()
            {
                if (!acceptor_strand_.running_in_this_thread())
                    return dispatch(acceptor_strand_, [this, self = shared_from_this()]{ execute_accept(); });
                acceptor_.async_accept([this, self = shared_from_this()](boost::system::error_code errc, boost::asio::ip::tcp::socket socket)
                {
                    if (errc) return close_acceptor(errc);
                    post(*execution_, [socket = std::make_shared<boost::asio::ip::tcp::socket>(std::move(socket)), this, self]() mutable { sockets_.push(std::move(socket)); });
                    execute_accept();
                });
            }

            void close_acceptor(boost::system::error_code errc)
            {
                fmt::print(std::cerr, "accept errc {}, errmsg {}\n", errc, errc.message());
                acceptor_.close();
            }
        };

        template class acceptor<boost::asio::ip::tcp>;
    }
}