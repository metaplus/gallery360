#pragma once

namespace net::client
{
    template<typename Protocal>
    class session;

    template<>
    class session<protocal::http>
        : detail::session_base<boost::asio::ip::tcp::socket>
    {
        using response_type = boost::beast::http::response<boost::beast::http::dynamic_body>;
        boost::asio::io_context::strand mutable strand_;
        boost::promise<response_type> response_promise_;
    public:
        session(boost::asio::ip::tcp::socket&& socket,
                boost::asio::io_context& context)
            : session_base(std::move(socket), context)
            , strand_(context_)
        {
            assert(socket_.is_open());
            fmt::print(std::cout, "session: socket peer endpoint {}/{}\n", socket_.local_endpoint(), socket_.remote_endpoint());
        }

        bool operator<(session<protocal::http> const& that) const
        {
            return std::less<boost::asio::basic_socket<boost::asio::ip::tcp>>{}(socket_, that.socket_);
        }

        using session_base::local_endpoint;
        using session_base::remote_endpoint;

        boost::future<
            boost::beast::http::response<boost::beast::http::dynamic_body>
        > async_send(std::string_view host, std::string_view target)
        {
            namespace http = boost::beast::http;
            http::request<http::empty_body> request;
            request.version(protocal::http::default_version);
            request.method(protocal::http::default_method);
            request.target(target.data());
            request.set(http::field::host, host.data());
            request.set(http::field::user_agent, "MetaPlus");
            return async_send(std::move(request));
        }

        template<typename SendBody>
        boost::future<
            boost::beast::http::response<boost::beast::http::dynamic_body>
        > async_send(boost::beast::http::request<SendBody> request)
        {
            namespace http = boost::beast::http;
            static_assert(boost::beast::http::is_body<SendBody>::value);
            fmt::print("session: async_send\n");
            response_promise_ = {};
            http::async_write(socket_, request, on_send_request());
            auto const inactive = is_active(true);
            assert(!inactive);
            return response_promise_.get_future();
        }

    private:
        folly::Function<void(boost::system::error_code, std::size_t) const>
        on_send_request()
        {
            namespace http = boost::beast::http;
            return [this](boost::system::error_code errc, std::size_t transfer_size)
            {
                fmt::print("session: on_send_request, errc {}, transfer {}\n", errc, transfer_size);
                if (errc)
                    return fail_promise_then_close(response_promise_, errc);
                auto response = std::make_unique<response_type>();
                auto& response_ref = *response;
                if (is_chunked())
                    throw core::not_implemented_error{ "" };
                http::async_read(socket_, recvbuf_, response_ref, on_recv_response(std::move(response)));
            };
        }

        folly::Function<void(boost::system::error_code, std::size_t)>
        on_recv_response(std::unique_ptr<response_type> response)
        {
            return [this, response = std::move(response)](boost::system::error_code errc, std::size_t transfer_size) mutable
            {
                fmt::print("session: on_recv_response, errc {}, transfer {}\n", errc, transfer_size);
                if (errc)
                    return fail_promise_then_close(response_promise_, errc);
                response_promise_.set_value(std::move(*response.release()));
            };
        }
    };
}
