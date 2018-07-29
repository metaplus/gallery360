#pragma once

namespace net::client
{
    template<typename Protocal, typename Policy>
    class session;

    template<typename ResponseBody>
    class session<protocal::http, policy<ResponseBody>>
        : detail::session_base<boost::asio::ip::tcp::socket>
        , protocal::http::protocal_base
    {
        using response_body = ResponseBody;

        std::unique_ptr<promise_base<response_type<response_body>>> response_promise_;
        mutable std::atomic<int64_t> current_index_ = -1;

    public:
        session(boost::asio::ip::tcp::socket&& socket, boost::asio::io_context& context)
            : session_base(std::move(socket), context)
        {
            assert(socket_.is_open());
            fmt::print(std::cout, "session: socket peer endpoint {}/{}\n", socket_.local_endpoint(), socket_.remote_endpoint());
            reserve_recvbuf_capacity();
        }

        bool operator<(session<protocal::http, policy<ResponseBody>> const& that) const
        {
            return std::less<boost::asio::basic_socket<boost::asio::ip::tcp>>{}(socket_, that.socket_);
        }

        using session_base::local_endpoint;
        using session_base::remote_endpoint;

        template<typename RequestBody, template<typename> typename Promise>
        int64_t async_send_request(request_type<RequestBody>&& request, Promise<response_body>&& promise)
        {
            namespace http = boost::beast::http;
            static_assert(http::is_body<RequestBody>::value);
            fmt::print("session: async_send_request\n");
            response_promise_ = promise_base<response_body>::from(std::move(promise));
            auto request_ptr = std::make_unique<http::request<RequestBody>>(std::move(request));
            auto& request_ref = *request_ptr;
            http::async_write(socket_, request_ref, on_send_request(std::move(request_ptr)));
            auto const inactive = is_active(true);
            assert(!inactive);
            return std::atomic_fetch_add(&current_index_, 1);
        }

    private:
        template<typename RequestBody>
        folly::Function<void(boost::system::error_code, std::size_t)>
            on_send_request(std::unique_ptr<request_type<RequestBody>> request)
        {
            namespace http = boost::beast::http;
            return[this, guard = std::move(request)](boost::system::error_code errc, std::size_t transfer_size)
            {
                fmt::print("session: on_send_request, errc {}, transfer {}\n", errc, transfer_size);
                if (errc)
                    return fail_promise_then_close_socket(*response_promise_, errc);
                auto response = std::make_unique<response_type>();
                auto& response_ref = *response;
                if (is_chunked())
                    throw core::not_implemented_error{ "" };
                http::async_read(socket_, recvbuf_, response_ref, on_recv_response(std::move(response)));
            };
        }

        folly::Function<void(boost::system::error_code, std::size_t)>
            on_recv_response(std::unique_ptr<response_type<response_body>> response)
        {
            return[this, response = std::move(response)](boost::system::error_code errc, std::size_t transfer_size) mutable
            {
                fmt::print("session: on_recv_response, errc {}, transfer {}\n", errc, transfer_size);
                if (errc)
                    return fail_promise_then_close_socket(*response_promise_, errc);
                response_promise_->set_value(std::move(*response.release()));
            };
        }
    };
}
