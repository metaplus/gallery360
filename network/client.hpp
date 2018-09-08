#pragma once

namespace net::client
{
    using default_policy = policy<dynamic_body>;

    struct info
    {
        static int16_t net_session_index();
        static std::shared_ptr<spdlog::logger> create_logger(int16_t index);
    };

    template<typename Protocal, typename Policy = default_policy>
    class session;

    template<>
    class session<protocal::http, policy<dynamic_body>>
        : detail::session_base<boost::asio::ip::tcp::socket, multi_buffer>
        , protocal::http::protocal_base
    {
        using response_body = dynamic_body;
        using response = response_type<response_body>;
        using response_parser = response_parser_type<response_body>;

        boost::promise<response> promise_response_;
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

        template<typename RequestBody>
        boost::future<response> async_send_request(request_type<RequestBody>&& request) {
            static_assert(boost::beast::http::is_body<RequestBody>::value);
            logger_->info("async_send_request via message");
            promise_response_ = prepare_response_parser();
            auto request_ptr = std::make_unique<request_type<RequestBody>>(std::move(request));
            auto& request_ref = *request_ptr;
            boost::beast::http::async_write(
                socket_, request_ref, on_send_request(std::move(request_ptr)));
            core::check[false] << is_active(true);
            std::atomic_fetch_add(&round_trip_index_, 1);
            return promise_response_.get_future();
        }

        folly::Function<multi_buffer()>
            transform_buffer_stream(folly::Function<std::string()> url_supplier);

    private:
        boost::promise<response> prepare_response_parser();

        folly::Function<void(boost::system::error_code, std::size_t)>
            on_send_request(request_ptr<empty_body> request);

        folly::Function<void(boost::system::error_code, std::size_t)>
            on_recv_response();
    };

    template<typename Protocal, typename Policy = default_policy>
    using session_ptr = std::unique_ptr<session<Protocal, Policy>>;
    using http_session = session<protocal::http, policy<dynamic_body>>;
}
