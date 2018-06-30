#pragma once

namespace net::server
{
    template<typename Protocal, typename Socket = boost::asio::ip::tcp::socket>
    class session;

    template<>
    class session<protocal::http, boost::asio::ip::tcp::socket> : public executor_guard
    {
        using request_type = boost::beast::http::request<boost::beast::http::string_body>;
        using request_container = std::map<std::chrono::steady_clock::time_point, request_type>;

        std::filesystem::path const root_path_;
        boost::asio::ip::tcp::socket socket_;
        request_container requests_;
        boost::beast::flat_buffer recvbuf_;
        boost::asio::io_context::strand mutable session_strand_;
        boost::asio::io_context::strand mutable process_strand_;
        bool const chunked_{ false };
        std::atomic<bool> mutable active_{ false };
        std::atomic<size_t> mutable request_consume_{ 0 };

    public:
        session(boost::asio::ip::tcp::socket sock, executor_guard guard, std::filesystem::path root)
            : executor_guard(std::move(guard))
            , root_path_(std::move(root))
            , socket_(std::move(sock))
            , session_strand_(sock.get_executor().context())
            , process_strand_(sock.get_executor().context())
        {
            assert(socket_.is_open());
            assert(std::filesystem::is_directory(root_path_));
            fmt::print(std::cout, "session: socket peer endpoint {}/{}\n", socket_.local_endpoint(), socket_.remote_endpoint());
            fmt::print(std::cout, "session: file root path {}\n", root_path_);
        }

        session(boost::asio::ip::tcp::socket sock, executor_guard guard, std::filesystem::path root, use_chunk_t)
            : session(std::move(sock), std::move(guard), std::move(root))
        {
            const_cast<bool&>(chunked_) = true;
        }

        session<protocal::http, boost::asio::ip::tcp::socket>& async_run()
        {
            if (!std::atomic_exchange(&active_, true))
                boost::asio::post(session_strand_, [this] { exec_recv(); });
            return *this;
        }

        bool operator<(session<protocal::http, boost::asio::ip::tcp::socket> const& that) const
        {
            return std::less<boost::asio::basic_socket<boost::asio::ip::tcp>>{}(socket_, that.socket_);
        }

        boost::asio::ip::tcp::endpoint local_endpoint() const
        {
            return socket_.local_endpoint();
        }

        boost::asio::ip::tcp::endpoint remote_endpoint() const
        {
            return socket_.local_endpoint();
        }

    private:
        void exec_recv()
        {
            assert(session_strand_.running_in_this_thread());
            erase_consumed_request();
            auto& request = insert_empty_request()->second;
            fmt::print("session: wait incoming request, recvbuf size {}\n", recvbuf_.size());
            boost::beast::http::async_read(
                socket_, recvbuf_, request,
                bind_executor(session_strand_, [&request, this](boost::system::error_code errc, std::size_t transfer_size)
                              {
                                  fmt::print(std::cout, "session: handle recv errc {}, transfer {}\n", errc, transfer_size);
                                  if (errc) close_socket(errc, boost::asio::socket_base::shutdown_both);
                                  boost::asio::post(process_strand_, [&request, this]
                                                    {
                                                        fmt::print(std::cout, "session: request head {}\n", request.base());
                                                        fmt::print(std::cout, "session: request body {}\n", request.body());
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
            assert(session_strand_.running_in_this_thread());
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
            fmt::print(std::cout, "session: response head {}\n", response_ref.base());
            http::async_write(socket_, response_ref,
                              bind_executor(session_strand_, [this, response = std::move(response)](boost::system::error_code errc, size_t transfer_size)
            {
                fmt::print(std::cout, "session: handle send errc {}, last {}, transfer {}\n", errc, response->need_eof(), transfer_size);
                if (errc || response->need_eof()) return close_socket(errc, boost::asio::socket_base::shutdown_send);
                boost::asio::post(session_strand_, [this] { exec_recv(); });
            }));
        }

        class read_chunk_context : boost::noncopyable
        {
            std::filesystem::path const file_path_;
            std::list<std::vector<char>> chunk_buflist_;
            boost::mutex mutable buflist_mutex_;
            boost::condition_variable mutable buflist_condvar_;
            boost::thread mutable read_thread_;

        public:
            explicit read_chunk_context(std::filesystem::path path)
                : file_path_(std::move(path))
                , read_thread_(&read_chunk_context::read_until_eof, this)
            {}

            explicit read_chunk_context(std::string_view path)
                : read_chunk_context(std::filesystem::path{ path })
            {}

            std::optional<std::vector<char>> get_chunk()
            {
                if (read_thread_.interruption_requested())
                    return std::nullopt;
                boost::unique_lock<boost::mutex> buflist_lock{ buflist_mutex_ };
                if (chunk_buflist_.empty())
                    buflist_condvar_.wait(buflist_lock);
                std::optional<std::vector<char>> chunk{ std::move(chunk_buflist_.front()) };
                chunk_buflist_.pop_front();
                return chunk;
            }

            ~read_chunk_context()
            {
                read_thread_.interrupt();
                if (read_thread_.joinable()) read_thread_.join();
            }

        private:
            void read_until_eof()
            {
                boost::beast::file file;
                boost::system::error_code errc;
                size_t total_read_size{ 0 };
                try
                {
                    file.open(file_path_.generic_string().c_str(), boost::beast::file_mode::scan, errc);
                    assert(file.is_open());
                    assert(!errc);
                    while (!errc && !read_thread_.interruption_requested())
                    {
                        std::vector<char> chunk(default_max_chunk_size);
                        auto const read_size = file.read(chunk.data(), chunk.size(), errc);
                        if (read_size < chunk.size())
                            chunk.resize(read_size);
                        {
                            boost::lock_guard<boost::mutex> lock_guard{ buflist_mutex_ };
                            chunk_buflist_.push_back(std::move(chunk));
                        }
                        buflist_condvar_.notify_one();
                        total_read_size += read_size;
                    }
                    boost::lock_guard<boost::mutex> lock_guard{ buflist_mutex_ };
                    chunk_buflist_.emplace_back();
                } catch (...)
                {
                    fmt::print(std::cerr, "read_chunk_context: read_until_eof errc{}, errmsg{}\n", errc, errc.message());
                }
            }
        };

        void exec_send_header(request_type& request, use_chunk_t)
        {
            namespace http = boost::beast::http;
            assert(session_strand_.running_in_this_thread());
            auto response = std::make_shared<http::response<http::empty_body>>(http::status::ok, request.version());
            auto serializer = std::make_shared<http::response_serializer<http::empty_body>>(*response);
            auto& serializer_ref = *serializer;
            response->set(http::field::server, "METAPLUS");
            response->chunked(true);
            auto const read_context = std::make_shared<read_chunk_context>(concat_target_path(request.target()));
            http::async_write_header(
                socket_, serializer_ref,
                bind_executor(session_strand_, [response, serializer, read_context, this](boost::system::error_code errc, size_t transfer_size)
                              {
                                  fmt::print("session: handle send header errc {}, transfer {}\n", errc, transfer_size);
                                  if (errc) return close_socket(errc, boost::asio::socket_base::shutdown_send);
                                  exec_send_chunk(read_context);
                              }));
        }

        void exec_send_chunk(std::shared_ptr<read_chunk_context> read_context)
        {
            namespace http = boost::beast::http;
            assert(session_strand_.running_in_this_thread());
            auto const chunk = read_context->get_chunk();
            if (!chunk.has_value() || chunk->empty())
            {
                [[maybe_unused]] auto const write_size = boost::asio::write(socket_, http::make_chunk_last());
                fmt::print("session: last chunk written, transfer size {}, has_value {}, empty {}\n",
                           write_size, chunk.has_value(), chunk.has_value() && chunk->empty());
                read_context = nullptr;
                return std::atomic_store(&active_, false);
                // return socket_.shutdown(boost::asio::socket_base::shutdown_both);
                // return exec_recv();
            }
            auto& chunk_ref = *chunk;
            boost::asio::async_write(
                socket_, http::make_chunk(boost::asio::buffer(chunk_ref)),
                bind_executor(session_strand_, [chunk, read_context, this](boost::system::error_code errc, size_t transfer_size)
                              {
                                  fmt::print(std::cout, "session: handle send errc {}, transfer {}\n", errc, transfer_size);
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
            fmt::print(std::cerr, "session: socket close, errc {}, errmsg {}\n", errc, errc.message());
            // socket_.cancel();
            // socket_.shutdown(operation);
            erase_consumed_request();
        }
    };

    static_assert(!std::is_move_constructible<session<protocal::http, boost::asio::ip::tcp::socket>>::value);
}
