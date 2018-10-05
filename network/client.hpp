#pragma once

namespace net::client
{
    struct info
    {
        static int16_t net_session_index();
        static std::shared_ptr<spdlog::logger> create_logger(int16_t index);
    };

    template<typename Protocal>
    class session;

    template<typename Protocal>
    using session_ptr = std::unique_ptr<session<Protocal>>;
    using http_session = session<protocal::http>;
    using http_session_ptr = std::unique_ptr<session<protocal::http>>;

    template<>
    class session<protocal::http>
        : detail::session_base<boost::asio::ip::tcp::socket, multi_buffer>
        , protocal::http::protocal_base
    {
        using response = response<dynamic_body>;
        using response_parser = response_parser<dynamic_body>;

        folly::Promise<response> response_;
        std::optional<response_parser> response_parser_;
        const int16_t index_ = info::net_session_index();
        const std::shared_ptr<spdlog::logger> logger_ = info::create_logger(index_);

    public:
        session(socket_type&& socket, boost::asio::io_context& context);

        session() = delete;
        session(const session&) = delete;
        session& operator=(const session&) = delete;

        using session_base::operator<;
        using session_base::local_endpoint;
        using session_base::remote_endpoint;

        template<typename Body>
        folly::SemiFuture<response> async_send_request(request<Body>&& req) {
            static_assert(boost::beast::http::is_body<Body>::value);
            logger_->info("async_send_request via message");
            auto[promise_response, future_response] = folly::makePromiseContract<response>();
            response_ = std::move(promise_response);
            config_response_parser();
            auto request_ptr = folly::makeMoveWrapper(std::make_unique<request<Body>>(std::move(req)));
            auto& request_ref = **request_ptr;
            boost::beast::http::async_write(
                socket_, request_ref,
                [this, request_ptr](boost::system::error_code errc, std::size_t transfer_size) {
                    logger_->info("on_send_request errc {} transfer {}", errc, transfer_size);
                    if (errc) {
                        logger_->error("on_send_request failure");
                        return close_promise_and_socket(response_, errc, boost::asio::socket_base::shutdown_send);
                    }
                    boost::beast::http::async_read(socket_, recvbuf_, *response_parser_, on_recv_response());
                });
            core::check[false] << is_active(true);
            round_trip_index_++;
            return std::move(future_response);
        }

        template<typename Target, typename Body>
        folly::SemiFuture<Target> async_send_request_for(request<Body>&& req) {
            static_assert(!std::is_reference<Target>::value);
            return async_send_request(std::move(req))
                .deferValue(
                    [](response&& response) -> Target {
                        if (response.result() != boost::beast::http::status::ok) {
                            core::throw_bad_request(response.reason().data());
                        }
                        if constexpr (std::is_same<multi_buffer, Target>::value) {
                            return std::move(response).body();
                        }
                        core::throw_unreachable("wrong type");
                    });
        }

        folly::Function<multi_buffer()>
            transform_buffer_stream(folly::Function<std::string()> url_supplier);

        static http_session_ptr create(socket_type&& socket, boost::asio::io_context& context);

    private:
        void config_response_parser();

        folly::Function<void(boost::system::error_code, std::size_t)> on_recv_response();
    };
}
