#include "stdafx.h"
#include "client.hpp"

namespace net::client
{
    namespace http = boost::beast::http;

    auto make_logger = core::console_logger_factory("net.client.session");

    http_session::session(socket_type&& socket,
                          boost::asio::io_context& context)
        : session_base(std::move(socket), context) {
        assert(socket_.is_open());
        std::tie(core::as_mutable(index_),
                 core::as_mutable(logger_)) = make_logger();
        core::as_mutable(identity_) = fmt::format("http_session${}", index_);
        #ifdef NDEBUG
        logger_->set_level(spdlog::level::warn);
        #endif
        logger_->info("constructor socket endpoint client {} server {}", socket_.local_endpoint(), socket_.remote_endpoint());
        reserve_recvbuf_capacity();
    }

    http_session_ptr session<protocal::http>::create(socket_type&& socket,
                                                     boost::asio::io_context& context) {
        return std::make_unique<http_session>(std::move(socket),
                                              context);
    }

    void http_session::config_response_parser() {
        response_parser_.emplace()
                        .body_limit(std::numeric_limits<uint64_t>::max());
    }

    auto http_session::on_recv_response(int64_t index) {
        return [=](boost::system::error_code errc,
                   std::size_t transfer_size) mutable {
            logger_->info("on_recv_response errc {} transfer {}", errc, transfer_size);
            if (errc) {
                logger_->error("on_recv_response failure");
                return shutdown_and_reject_request(core::bad_response_error{ errc.message() },
                                                   errc, boost::asio::socket_base::shutdown_receive);
            }
            if (response_parser_->get().result() != boost::beast::http::status::ok) {
                logger_->error("on_recv_response bad response");
                return shutdown_and_reject_request(core::bad_response_error{ response_parser_->get().reason().data() },
                                                   errc, boost::asio::socket_base::shutdown_receive);
            }
            trace_event("response=recv:index={}:transfer={}", index, transfer_size);
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

    auto http_session::on_send_request(int64_t index, std::any&& request) {
        return [=, request = std::move(request)](boost::system::error_code errc,
                                                 std::size_t transfer_size) mutable {
            logger_->info("on_send_request errc {} transfer {}", errc, transfer_size);
            if (errc) {
                logger_->error("on_send_request failure");
                return shutdown_and_reject_request(core::bad_request_error{ errc.message() },
                                                   errc, boost::asio::socket_base::shutdown_send);
            }
            trace_event("request=send:index={}:transfer={}", index, transfer_size);
            http::async_read(socket_, recvbuf_,
                             *response_parser_, on_recv_response(index));
        };
    }

    void http_session::trace_by(trace_callback callback) {
        trace_callback_ = [this, callback = std::move(callback)](std::string event) mutable {
            callback(identity_, std::move(event));
        };
    }

    auto http_session::send_request(request<empty_body>&& request)
    -> folly::SemiFuture<response<dynamic_body>> {
        logger_->info("send_request empty body");
        auto [promise_response, future_response] = folly::makePromiseContract<response<dynamic_body>>();
        request_list_.withWLock(
            [this, &request, &promise_response](request_list& request_list) {
                auto request_index = ++round_index_;
                auto request_target = request.target().to_string();
                auto request_satisfy = [=,
                        promise_response = std::move(promise_response),
                        request = std::move(request)
                    ](request_param request_param) mutable {
                    using request_type = base::request<empty_body>;
                    using response_type = base::response<dynamic_body>;
                    core::visit(
                        request_param,
                        [=, &request](std::monostate) {
                            config_response_parser();
                            auto request_ptr = std::make_shared<request_type>(std::move(request));
                            auto& request_ref = *request_ptr;
                            trace_event("request=ready:index={}", request_index);
                            http::async_write(socket_, request_ref,
                                              on_send_request(request_index, std::move(request_ptr)));
                        },
                        [&promise_response](auto& response) {
                            using param_type = std::decay_t<decltype(response)>;
                            if constexpr (std::is_same<response_type, param_type>::value) {
                                return promise_response.setValue(std::move(response));
                            } else if constexpr (std::is_base_of<std::exception, param_type>::value) {
                                return promise_response.setException(response);
                            }
                            core::throw_unreachable("send_request_task");
                        });
                };
                if (active_) {
                    trace_event("request=pend:index={}:target={}", request_index, request_target);
                    request_list.push_back(std::move(request_satisfy));
                    if (std::size(request_list) == 1) {
                        request_list.front()(std::monostate{});
                    }
                } else {
                    request_satisfy(core::session_closed_error{ "send_request_task" });
                }
            });
        return std::move(future_response);
    }
}
