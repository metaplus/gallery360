#pragma once

namespace net
{
    namespace v2
    {
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

                std::shared_ptr<boost::asio::io_context> const execution_;
                bool const chunked_{ false };
                std::filesystem::path const root_path_;
                boost::asio::ip::tcp::socket socket_;
                request_container requests_;
                boost::beast::flat_buffer recvbuf_;
                boost::asio::io_context::strand mutable session_strand_;
                boost::asio::io_context::strand mutable process_strand_;
                std::atomic<bool> mutable active_{ false };
                std::atomic<size_t> mutable request_consume_{ 0 };

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
                    assert(is_directory(root_path_));
                    fmt::print(std::cout, "socket peer endpoint {}/{}\n", socket_.local_endpoint(), socket_.remote_endpoint());
                    fmt::print(std::cout, "file root path {}\n", root_path_);
                }

                session(boost::asio::ip::tcp::socket sock, std::shared_ptr<boost::asio::io_context> ctx)
                    : session(std::move(sock), std::move(ctx), net::config().get<std::string>("net.server.directories.root"))
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

                session<protocal::http, boost::asio::ip::tcp::socket>& async_run()
                {
                    if (!std::atomic_exchange(&active_, true)) exec_recv();
                    return *this;
                }

            private:
                void exec_recv()
                {
                    if (!session_strand_.running_in_this_thread())
                        return dispatch(session_strand_, [this, self = shared_from_this()]{ exec_recv(); });
                    erase_consumed_request();
                    auto& request = insert_empty_request()->second;
                    fmt::print("wait incoming request, recvbuf size {}\n", recvbuf_.size());
                    boost::beast::http::async_read(socket_, recvbuf_, request, bind_executor(session_strand_,
                                                                                             [&request, this, self = shared_from_this()](boost::system::error_code errc, std::size_t transfer_size)
                    {
                        fmt::print(std::cout, "handle recv errc {}, transfer {}\n", errc, transfer_size);
                        if (errc) close_socket(errc, boost::asio::socket_base::shutdown_both);
                        boost::asio::post(process_strand_, [&request, this, self]
                                          {
                                              fmt::print(std::cout, "request head {}\n", request.base());
                                              fmt::print(std::cout, "request body {}\n", request.body());
                                              // TODO: external process routine towards request
                                              std::atomic_fetch_add(&request_consume_, 1);
                                          });
                        if (chunked_) exec_send_header(request, use_chunk);
                        else exec_send(request);
                    }));
                }

                void exec_send(request_type& request)
                {
                    namespace http = boost::beast::http;
                    if (!session_strand_.running_in_this_thread())
                        return dispatch(session_strand_, [&request, this, self = shared_from_this()]{ exec_send(request); });
                    auto const target_path = concat_target_path(request.target());
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
                    http::async_write(socket_, response_ref,
                                      bind_executor(session_strand_,
                                                    [response = std::move(response), this, self = shared_from_this()
                                                    ](boost::system::error_code errc, size_t transfer_size)
                    {
                        fmt::print(std::cout, "handle send errc {}, last {}, transfer {}\n", errc, response->need_eof(), transfer_size);
                        if (errc || response->need_eof()) return close_socket(errc, boost::asio::socket_base::shutdown_send);
                        exec_recv();
                    }));
                }

                class read_chunk_context : core::noncopyable
                {
                    const std::filesystem::path  file_path_;
                    tbb::concurrent_bounded_queue<std::shared_ptr<std::vector<char>>> chunk_queue_;
                    mutable boost::thread read_thread_;

                public:
                    explicit read_chunk_context(std::filesystem::path path)
                        : file_path_(std::move(path))
                        , read_thread_(&read_chunk_context::read_until_eof, this)
                    {}

                    explicit read_chunk_context(std::string_view path)
                        : read_chunk_context(std::filesystem::path{ path })
                    {}

                    std::shared_ptr<std::vector<char>> get_chunk()
                    {
                        std::shared_ptr<std::vector<char>> chunk;
                        chunk_queue_.pop(chunk);
                        return chunk;
                    }

                    ~read_chunk_context()
                    {
                        if (read_thread_.joinable()) read_thread_.join();
                    }

                private:
                    void read_until_eof()
                    {
                        boost::beast::file file;
                        boost::system::error_code errc;
                        file.open(file_path_.generic_string().c_str(), boost::beast::file_mode::scan, errc);
                        chunk_queue_.set_capacity(default_max_chunk_quantity);
                        assert(!errc && file.is_open());
                        size_t read_size{ 0 };
                        while (!errc)
                        {
                            auto const chunk = std::make_shared<std::vector<char>>(default_max_chunk_size);
                            auto const chunk_size = file.read(chunk->data(), chunk->size(), errc);
                            if (chunk_size < chunk->size()) chunk->resize(chunk_size);
                            chunk_queue_.push(std::move(chunk));
                            read_size += chunk_size;
                        }
                        chunk_queue_.emplace();
                    }
                };

                void exec_send_header(request_type& request, use_chunk_t)
                {
                    namespace http = boost::beast::http;
                    if (!session_strand_.running_in_this_thread())
                        return dispatch(session_strand_, [&request, this, self = shared_from_this()]{ exec_send_header(request,use_chunk); });
                    auto response = std::make_shared<http::response<http::empty_body>>(http::status::ok, request.version());
                    auto serializer = std::make_shared<http::response_serializer<http::empty_body>>(*response);
                    auto& serializer_ref = *serializer;
                    response->set(http::field::server, "METAPLUS");
                    response->chunked(true);
                    auto const read_context = std::make_shared<read_chunk_context>(concat_target_path(request.target()));
                    http::async_write_header(socket_, serializer_ref,
                                             bind_executor(session_strand_,
                                                           [response, serializer, read_context, this, self = shared_from_this()
                                                           ](boost::system::error_code errc, size_t transfer_size)
                    {
                        fmt::print("handle send header errc {}, transfer {}\n", errc, transfer_size);
                        if (errc) return close_socket(errc, boost::asio::socket_base::shutdown_send);
                        exec_send_chunk(read_context);
                    }));
                }

                void exec_send_chunk(std::shared_ptr<read_chunk_context> read_context)
                {
                    namespace http = boost::beast::http;
                    if (!session_strand_.running_in_this_thread())
                        return dispatch(session_strand_, [read_context, this, self = shared_from_this()]{ exec_send_chunk(read_context); });
                    auto const chunk = read_context->get_chunk();
                    if (!chunk || chunk->empty())
                    {
                        [[maybe_unused]] auto const write_size = boost::asio::write(socket_, http::make_chunk_last());
                        fmt::print("last chunk written, transfer size {}, ptr {}, empty {}\n", write_size, chunk != nullptr, chunk != nullptr && chunk->empty());
                        read_context = nullptr;
                        std::atomic_store(&active_, false);
                        return socket_.shutdown(boost::asio::socket_base::shutdown_both);
                        // return exec_recv();
                    }
                    auto& chunk_ref = *chunk;
                    boost::asio::async_write(
                        socket_, http::make_chunk(boost::asio::buffer(chunk_ref)),
                        bind_executor(session_strand_, [chunk, read_context, this, self = shared_from_this()](boost::system::error_code errc, size_t transfer_size)
                    {
                        fmt::print(std::cout, "handle send errc {}, transfer {}\n", errc, transfer_size);
                        if (errc) return close_socket(errc, boost::asio::socket_base::shutdown_send);
                        exec_send_chunk(read_context);
                    }));
                }

                std::filesystem::path concat_target_path(boost::beast::string_view request_target) const
                {
                    return std::filesystem::path{ root_path_ }.concat(request_target.begin(), request_target.end());
                }

                request_container::iterator insert_empty_request()
                {
                    auto const[iterator, success] = requests_.emplace(std::chrono::steady_clock::now(), request_container::mapped_type{});
                    assert(success);
                    return iterator;
                }

                size_t erase_consumed_request()
                {
                    auto const consume_size = std::atomic_exchange(&request_consume_, 0);
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
                std::shared_ptr<boost::asio::io_context> const execution_;
                boost::asio::ip::tcp::acceptor acceptor_;
                tbb::concurrent_bounded_queue<std::shared_ptr<boost::asio::ip::tcp::socket>> sockets_;
                std::atomic<bool> mutable active_{ false };

            public:
                acceptor(boost::asio::ip::tcp::endpoint endpoint, std::shared_ptr<boost::asio::io_context> ctx, bool reuse_addr = false)
                    : execution_(std::move(ctx))
                    , acceptor_(*execution_, endpoint, reuse_addr)
                {
                    assert(acceptor_.is_open());
                    fmt::print("acceptor listen address {}, port {}\n", endpoint.address(), listen_port());
                }

                uint16_t listen_port() const
                {
                    return acceptor_.local_endpoint().port();
                }

                acceptor<boost::asio::ip::tcp>& async_run()
                {
                    if (!std::atomic_exchange(&active_, true))
                        boost::asio::post(acceptor_.get_executor(), std::bind(&acceptor<boost::asio::ip::tcp>::exec_accept, shared_from_this()));
                    return *this;
                }

                template<typename ApplicationProtocal, typename ...Types>
                std::shared_ptr<session<ApplicationProtocal, boost::asio::ip::tcp::socket>> listen_session(Types&& ...args)
                {
                    async_run();
                    std::shared_ptr<boost::asio::ip::tcp::socket> socket;
                    sockets_.pop(socket);
                    return std::make_shared<session<ApplicationProtocal, boost::asio::ip::tcp::socket>>(std::move(*socket), execution_, std::forward<Types>(args)...);
                }

            private:
                void exec_accept()
                {
                    acceptor_.async_accept([this, self = shared_from_this()](boost::system::error_code errc, boost::asio::ip::tcp::socket socket)
                    {
                        if (errc) return close_acceptor(errc);
                        boost::asio::post(acceptor_.get_executor(), std::bind(&acceptor<boost::asio::ip::tcp>::exec_accept, shared_from_this()));
                        // boost::asio::post(acceptor_.get_executor(),
                        //                   [socket = std::make_shared<boost::asio::ip::tcp::socket>(std::move(socket)), this, self
                        //                   ]() mutable { sockets_.push(std::move(socket)); });
                        sockets_.push(std::make_shared<boost::asio::ip::tcp::socket>(std::move(socket)));
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

    namespace server = v2::server;
}
