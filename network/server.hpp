#pragma once

namespace net::server
{
    using default_policy = policy<string_body>;

    template<typename Protocal, typename Policy = default_policy>
    class session;

    template<typename RequestBody>
    class session<protocal::http, policy<RequestBody>>
        : detail::session_base<boost::asio::ip::tcp::socket, boost::beast::flat_buffer>
        , protocal::http::protocal_base
    {
        using request_body = RequestBody;

        const std::filesystem::path root_path_;
        mutable boost::asio::io_context::strand strand_;

    public:
        session(boost::asio::ip::tcp::socket&& socket, boost::asio::io_context& context,
                std::filesystem::path root, bool chunked = false)
            : session_base(std::move(socket), context)
            , root_path_(std::move(root))
            , strand_(socket.get_executor().context()) {
            assert(socket_.is_open());
            assert(std::filesystem::is_directory(root_path_));
            fmt::print("session: socket peer endpoint {}/{}\n", socket_.local_endpoint(), socket_.remote_endpoint());
            fmt::print("session: file root path {}\n", root_path_);
        }

        bool operator<(session<protocal::http, policy<RequestBody>> const& that) const {
            return std::less<boost::asio::basic_socket<boost::asio::ip::tcp>>{}(socket_, that.socket_);
        }

        using session_base::local_endpoint;
        using session_base::remote_endpoint;

        void async_run() {
            fmt::print("session: async_run\n");
            request_ptr<request_body> request_ptr = std::make_unique<request<request_body>>();
            auto& request_ref = *request_ptr;
            boost::beast::http::async_read(socket_, recvbuf_, request_ref, on_recv_request(std::move(request_ptr)));
        }

    private:

        template<typename Body>
        folly::Function<void(boost::system::error_code, std::size_t)>
            on_recv_request(request_ptr<Body> request) {
            namespace http = boost::beast::http;
            return[this, request = std::move(request)](boost::system::error_code errc, std::size_t transfer_size)
            {
                fmt::print("session: on_recv_request, errc {}, transfer {}\n", errc, transfer_size);
                fmt::print("session: on_recv_request, request head\n\t{}", request->base());
                fmt::print("session: on_recv_request, request body\n\t{}", request->body());
                if (errc || request->need_eof()) {
                    return close_socket(boost::asio::socket_base::shutdown_receive);
                }
                auto const target_path = concat_target_path(request->target());
                fmt::print(std::cout, "target file path {}, exists {}\n", target_path, exists(target_path));
                if (std::filesystem::exists(target_path)) {
                    boost::system::error_code file_errc;
                    file_body::value_type response_body;
                    response_body.open(target_path.generic_string().c_str(), boost::beast::file_mode::scan, file_errc);
                    assert(!file_errc);
                    auto response = std::make_unique<http::response<file_body>>(http::status::ok,
                                                                                request->version(),
                                                                                std::move(response_body));
                    response->set(http::field::server, "MetaPlus");
                    //response->set(http::field::content_type, "video/mp4");
                    response->content_length(response_body.size());
                    response->keep_alive(request->keep_alive());
                    auto& response_ref = *response;
                    fmt::print("session: on_recv_request, response head {}", response->base());
                    http::async_write(socket_, response_ref, on_send_response(std::move(response)));
                } else {
                    auto response = std::make_unique<http::response<empty_body>>(http::status::bad_request,
                                                                                 request->version());
                    auto& response_ref = *response;
                    http::async_write(socket_, response_ref, on_send_response(std::move(response)));
                }
            };
        }

        template<typename Body>
        folly::Function<void(boost::system::error_code, std::size_t)>
            on_send_response(response_ptr<Body> response) {
            return[this, response = std::move(response)](boost::system::error_code errc, std::size_t transfer_size) mutable
            {
                fmt::print("session: on_send_response, errc {}, last {}, transfer {}\n", errc, response->need_eof(), transfer_size);
                if (errc || response->need_eof()) {
                    fmt::print("session: on_send_response, message {}\n", errc.message());
                    return close_socket(boost::asio::socket_base::shutdown_send);
                }
                async_run();
            };
        }

        std::filesystem::path concat_target_path(boost::beast::string_view request_target) const {
            return std::filesystem::path{ root_path_ }.concat(request_target.begin(), request_target.end());
        }
    };

    template<typename Protocal, typename Policy = default_policy>
    using session_ptr = std::unique_ptr<session<Protocal, Policy>>;
}
