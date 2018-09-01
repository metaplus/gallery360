#pragma once

namespace net::client
{
    using default_policy = policy<boost::beast::http::dynamic_body>;

    template<typename Protocal, typename Policy = default_policy>
    class session;

    template<typename ResponseBody>
    class session<protocal::http, policy<ResponseBody>>
        : detail::session_base<boost::asio::ip::tcp::socket, boost::beast::multi_buffer>
        , protocal::http::protocal_base
    {
        using response_body = ResponseBody;
        using response = response_type<response_body>;
        using response_parser = response_parser_type<response_body>;

        boost::promise<response> promise_response_;
        std::optional<response_parser> response_parser_;

    public:
        session(boost::asio::ip::tcp::socket&& socket, boost::asio::io_context& context)
            : session_base(std::move(socket), context) {
            assert(socket_.is_open());
            fmt::print("session: socket peer endpoint {}/{}\n", socket_.local_endpoint(), socket_.remote_endpoint());
            reserve_recvbuf_capacity();
        }

        bool operator<(session<protocal::http, policy<ResponseBody>> const& that) const {
            return std::less<boost::asio::basic_socket<boost::asio::ip::tcp>>{}(socket_, that.socket_);
        }

        using session_base::local_endpoint;
        using session_base::remote_endpoint;

        template<typename RequestBody>
        boost::future<response> async_send_request(request_type<RequestBody>&& request) {
            namespace http = boost::beast::http;
            static_assert(http::is_body<RequestBody>::value);
            fmt::print("session: async_send_request via message\n");
            promise_response_ = boost::promise<response>{};
            response_parser_.emplace();
            response_parser_->body_limit(std::numeric_limits<uint64_t>::max());
            auto request_ptr = std::make_unique<http::request<RequestBody>>(std::move(request));
            auto& request_ref = *request_ptr;
            http::async_write(socket_, request_ref, on_send_request(std::move(request_ptr)));
            auto const inactive = is_active(true);
            std::atomic_fetch_add(&round_trip_index_, 1);
            return promise_response_.get_future();
        }

    private:

        template<typename RequestBody>
        folly::Function<void(boost::system::error_code, std::size_t)>
            on_send_request(std::unique_ptr<request_type<RequestBody>> request) {
            namespace http = boost::beast::http;
            return[this, request = std::move(request)](boost::system::error_code errc, std::size_t transfer_size) {
                fmt::print("session: on_send_request, errc {}, transfer {}\n", errc, transfer_size);
                if (errc)
                    return fail_promise_then_close_socket(promise_response_, errc, boost::asio::socket_base::shutdown_send);
                auto response = std::make_unique<response_type<response_body>>();
                auto& response_ref = *response;
                if (is_chunked())
                    throw core::not_implemented_error{ "" };
                http::async_read(socket_, recvbuf_, *response_parser_, on_recv_response());
            };
        }

        folly::Function<void(boost::system::error_code, std::size_t)>
            on_recv_response() {
            return[this](boost::system::error_code errc, std::size_t transfer_size) mutable {
                fmt::print("session: on_recv_response, errc {}, transfer {}\n", errc, transfer_size);
                if (errc)
                    return fail_promise_then_close_socket(promise_response_, errc, boost::asio::socket_base::shutdown_receive);
                promise_response_.set_value(response_parser_->release());
            };
        }
    };

    template<typename Protocal, typename Policy = default_policy>
    using session_ptr = std::unique_ptr<session<Protocal, Policy>>;
}
