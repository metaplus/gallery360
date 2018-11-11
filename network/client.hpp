#pragma once

namespace net::client
{
    template<typename Protocal>
    class session;

    template<typename Protocal>
    using session_ptr = std::unique_ptr<session<Protocal>>;
    using http_session = session<protocal::http>;
    using http_session_ptr = std::unique_ptr<session<protocal::http>>;

    template<>
    class session<protocal::http> : detail::session_base<boost::asio::ip::tcp::socket, multi_buffer>,
                                    protocal::http::protocal_base
    {
        using request_param = std::variant<
            std::monostate,
            response<dynamic_body>,
            core::bad_request_error,
            core::bad_response_error,
            core::session_closed_error>;
        using request_list = std::list<folly::Function<void(request_param)>>;

        folly::Synchronized<request_list> request_list_;
        std::optional<response_parser<dynamic_body>> response_parser_;
        const int64_t index_ = 0;
        const std::shared_ptr<spdlog::logger> logger_;
        mutable bool active_ = true;

    public:
        session(socket_type&& socket,
                boost::asio::io_context& context);

        session() = delete;
        session(const session&) = delete;
        session& operator=(const session&) = delete;

        using session_base::operator<;
        using session_base::local_endpoint;
        using session_base::remote_endpoint;

        template<typename Body>
        folly::SemiFuture<response<dynamic_body>> send_request(request<Body>&& request) {
            static_assert(boost::beast::http::is_body<Body>::value);
            logger_->info("send_request via message");
            auto [promise_response, future_response] = folly::makePromiseContract<response<dynamic_body>>();
            auto send_request_task = [this, promise_response = std::move(promise_response),
                    request = std::move(request)](request_param request_param) mutable {
                core::visit(
                    request_param,
                    [this, &request](std::monostate) {
                        config_response_parser();
                        auto request_ptr = std::make_shared<session::request<Body>>(std::move(request));
                        auto& request_ref = *request_ptr;
                        boost::beast::http::async_write(socket_, request_ref,
                                                        on_send_request(std::move(request_ptr)));
                    },
                    [&promise_response](auto& response) {
                        using param_type = std::decay_t<decltype(response)>;
                        if constexpr (std::is_same<session::response<dynamic_body>, param_type>::value) {
                            return promise_response.setValue(std::move(response));
                        } else if constexpr (std::is_base_of<std::exception, param_type>::value) {
                            return promise_response.setException(response);
                        }
                        core::throw_unreachable("send_request_task");
                    });
            };
            request_list_.withWLock(
                [this, &send_request_task](request_list& request_list) {
                    if (active_) {
                        request_list.push_back(std::move(send_request_task));
                        if (std::size(request_list) == 1) {
                            request_list.front()(std::monostate{});
                        }
                    } else {
                        send_request_task(core::session_closed_error{ "send_request_task" });
                    }
                });
            round_index_++;
            return std::move(future_response);
        }

        template<typename Target, typename Body>
        folly::SemiFuture<Target> send_request_for(request<Body>&& req) {
            static_assert(!std::is_reference<Target>::value);
            return send_request(std::move(req)).deferValue(
                [](response<dynamic_body>&& response) -> Target {
                    if (response.result() != boost::beast::http::status::ok) {
                        core::throw_bad_request("send_request_for");
                    }
                    if constexpr (std::is_same<multi_buffer, Target>::value) {
                        return std::move(response).body();
                    }
                    core::throw_unreachable(__FUNCTION__);
                });
        }

        static http_session_ptr create(socket_type&& socket,
                                       boost::asio::io_context& context);

    private:
        void config_response_parser();

        template<typename Exception>
        void shutdown_and_reject_request(Exception&& exception,
                                         boost::system::error_code errc,
                                         boost::asio::socket_base::shutdown_type operation) {
            request_list_.withWLock(
                [this, errc, operation, &exception](request_list& request_list) {
                    for (auto& request : request_list) {
                        request(std::forward<Exception>(exception));
                    }
                    request_list.clear();
                    close_socket(errc, operation);
                    active_ = false;
                });
        }

        auto on_recv_response() {
            return [this](boost::system::error_code errc, std::size_t transfer_size) mutable {
                logger_->info("on_recv_response errc {} transfer {}", errc, transfer_size);
                if (errc) {
                    logger_->error("on_recv_response failure");
                    return shutdown_and_reject_request(core::bad_response_error{ errc.message() },
                                                       errc,
                                                       boost::asio::socket_base::shutdown_receive);
                }
                if (response_parser_->get().result() != boost::beast::http::status::ok) {
                    logger_->error("on_recv_response bad response");
                    return shutdown_and_reject_request(core::bad_response_error{ response_parser_->get().reason().data() },
                                                       errc,
                                                       boost::asio::socket_base::shutdown_receive);
                }
                request_list_.withWLock(
                    [this, &errc](request_list& response_list) {
                        response_list.front()(response_parser_->release());
                        response_list.pop_front();
                        if (std::size(response_list)) {
                            response_list.front()(std::monostate{});
                        }
                    });
            };
        }

        auto on_send_request(std::any&& request) {
            return [this, request = std::move(request)](boost::system::error_code errc,
                                                        std::size_t transfer_size) mutable {
                logger_->info("on_send_request errc {} transfer {}", errc, transfer_size);
                if (errc) {
                    logger_->error("on_send_request failure");
                    return shutdown_and_reject_request(core::bad_request_error{ errc.message() },
                                                       errc,
                                                       boost::asio::socket_base::shutdown_send);
                }
                boost::beast::http::async_read(socket_,
                                               recvbuf_,
                                               *response_parser_,
                                               on_recv_response());
            };
        }
    };
}
