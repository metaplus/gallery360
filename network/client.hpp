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
        using request_param = std::variant<
            std::monostate,
            response<dynamic_body>,
            core::bad_request_error,
            core::bad_response_error,
            core::session_closed_error
        >;
        using request_list = std::list<folly::Function<void(request_param)>>;

        folly::Synchronized<request_list> request_list_;
        std::optional<response_parser<dynamic_body>> response_parser_;
        const int16_t index_ = info::net_session_index();
        const std::shared_ptr<spdlog::logger> logger_ = info::create_logger(index_);
        mutable bool active_ = true;

    public:
        session(socket_type&& socket, boost::asio::io_context& context);

        session() = delete;
        session(const session&) = delete;
        session& operator=(const session&) = delete;

        using session_base::operator<;
        using session_base::local_endpoint;
        using session_base::remote_endpoint;

        template<typename Body>
        folly::SemiFuture<response<dynamic_body>> async_send_request(request<Body>&& req) {
            static_assert(boost::beast::http::is_body<Body>::value);
            logger_->info("async_send_request via message");
            auto request_wrapper = folly::makeMoveWrapper(req);
            auto promise_response = folly::makeMoveWrapper(folly::Promise<response<dynamic_body>>{});
            auto future_response = promise_response->getSemiFuture();
            auto invoke_send_request = [this, request_wrapper, promise_response](request_param request_param) mutable {
                core::visit(request_param,
                            [this, &request_wrapper](std::monostate) {
                                config_response_parser();
                                auto request_ptr = std::make_shared<request<Body>>(request_wrapper.move());
                                auto& request_ref = *request_ptr;
                                auto target = request_ref.target();
                                boost::beast::http::async_write(socket_, request_ref,
                                                                on_send_request(folly::makeMoveWrapper(std::any{ std::move(request_ptr) })));
                            }, [&promise_response](auto& resp) {
                                using param_type = std::decay_t<decltype(resp)>;
                                if constexpr (std::is_same<response<dynamic_body>, param_type>::value) {
                                    return promise_response->setValue(std::move(resp));
                                } else if constexpr (std::is_base_of<std::exception, param_type>::value) {
                                    return promise_response->setException(resp);
                                }
                                core::throw_unreachable("invoke_send_request");
                            });
            };
            request_list_.withWLock(
                [this, &invoke_send_request](request_list& request_list) {
                    if (active_) {
                        request_list.push_back(std::move(invoke_send_request));
                        if (std::size(request_list) == 1) {
                            request_list.front()({});
                        }
                    } else {
                        invoke_send_request(core::session_closed_error{ "async_send_request" });
                    }
                });
            round_trip_index_++;
            return future_response;
        }

        template<typename Target, typename Body>
        folly::SemiFuture<Target> async_send_request_for(request<Body>&& req) {
            static_assert(!std::is_reference<Target>::value);
            return async_send_request(std::move(req))
                .deferValue(
                    [](response<dynamic_body>&& response) -> Target {
                        if (response.result() != boost::beast::http::status::ok) {
                            core::throw_bad_request("async_send_request_for");
                        }
                        if constexpr (std::is_same<multi_buffer, Target>::value) {
                            return std::move(response).body();
                        }
                        core::throw_unreachable("async_send_request_for");
                    });
        }

        static http_session_ptr create(socket_type&& socket, boost::asio::io_context& context);

    private:
        void config_response_parser();

        template<typename Exception>
        void clear_request_then_close(Exception&& exception,
                                      boost::system::error_code errc,
                                      boost::asio::socket_base::shutdown_type operation) {
            request_list_.withWLock(
                [this, errc, operation, &exception](request_list& request_list) {
                    for (auto& request : request_list) {
                        request(std::forward<Exception>(exception));
                    }
                    request_list.clear();
                    close_socket(errc, operation);
                    //assert(!socket_.is_open());
                    active_ = false;
                });
        }

        folly::Function<void(boost::system::error_code, std::size_t)> on_send_request(folly::MoveWrapper<std::any> request);

        folly::Function<void(boost::system::error_code, std::size_t)> on_recv_response();
    };
}
