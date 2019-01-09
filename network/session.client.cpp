#include "stdafx.h"
#include "session.client.hpp"

namespace net::client
{
    namespace http = boost::beast::http;

    auto make_logger = core::console_logger_factory("net.client.session", true);

    session<protocal::http>::session(socket_type&& socket,
                                     boost::asio::io_context& context)
        : session_base{ std::move(socket), context }
        , request_sequence_{ context_.get_executor() } {
        assert(socket_.is_open());
        std::tie(core::as_mutable(index_),
                 core::as_mutable(logger_)) = make_logger();
        core::as_mutable(identity_) = fmt::format("session${}", index_);
#ifdef NDEBUG
        logger_().set_level(spdlog::level::warn);
#endif
        logger_().info("constructor socket endpoint client {} server {}", socket_.local_endpoint(), socket_.remote_endpoint());
        reserve_recvbuf_capacity();
    }

    auto session<protocal::http>::create(socket_type&& socket,
                                         boost::asio::io_context& context) -> pointer {
        return std::make_unique<session<protocal::http>>(std::move(socket), context);
    }

    void session<protocal::http>::emplace_response_parser() {
        response_parser_.emplace()
                        .body_limit(std::numeric_limits<uint64_t>::max());
    }

    auto session<protocal::http>::on_recv_response(int64_t index) {
        return [=](boost::system::error_code errc,
                   std::size_t transfer_size) mutable {
            assert(request_sequence_.running_in_this_thread());
            logger_().info("on_recv_response errc {} transfer {}", errc, transfer_size);
            if (errc) {
                logger_().error("on_recv_response failure");
                return fail_request_then_close(core::bad_response_error{ errc.message() },
                                               errc, boost::asio::socket_base::shutdown_receive);
            }
            if (response_parser_->get().result() != http::status::ok) {
                logger_().error("on_recv_response bad response");
                return fail_request_then_close(core::bad_response_error{ response_parser_->get().reason().data() },
                                               errc, boost::asio::socket_base::shutdown_receive);
            }
            trace_event("response=recv:index={}:transfer={}", index, transfer_size);
            request_list_.front().second.setValue(response_parser_->release());
            request_list_.pop_front();
            if (!request_list_.empty()) {
                send_front_request();
            }
        };
    }

    auto session<protocal::http>::on_send_request(int64_t index) {
        return [=](boost::system::error_code errc,
                   std::size_t transfer_size) mutable {
            assert(request_sequence_.running_in_this_thread());
            logger_().info("on_send_request errc {} transfer {}", errc, transfer_size);
            if (errc) {
                logger_().error("on_send_request failure");
                return fail_request_then_close(core::bad_request_error{ errc.message() },
                                               errc, boost::asio::socket_base::shutdown_send);
            }
            trace_event("request=send:index={}:transfer={}", index, transfer_size);
            http::async_read(socket_, recvbuf_, *response_parser_,
                             boost::asio::bind_executor(request_sequence_, on_recv_response(index)));
        };
    }

    void session<protocal::http>::send_front_request() {
        assert(request_sequence_.running_in_this_thread());
        emplace_response_parser();
        auto request_index = ++round_index_;
        trace_event("request=ready:index={}", request_index);
        http::async_write(socket_, request_list_.front().first,
                          boost::asio::bind_executor(request_sequence_, on_send_request(request_index)));
    }

    void session<protocal::http>::trace_by(trace_callback callback) {
        trace_callback_ = [this, callback = std::move(callback)](std::string event) mutable {
            callback(identity_, std::move(event));
        };
    }

    auto session<protocal::http>::send_request(request<empty_body>&& request)
    -> folly::SemiFuture<response<dynamic_body>> {
        logger_().info("send_request empty body");
        auto [promise, future] = folly::makePromiseContract<response<dynamic_body>>();
        boost::asio::post(
            request_sequence_,
            [this, request = std::move(request), response = std::move(promise)]() mutable {
                auto request_target = request.target().to_string();
                if (active_) {
                    request_list_.emplace_back(std::move(request), std::move(response));
                    if (request_list_.size() == 1) {
                        send_front_request();
                    }
                } else {
                    response.setException(core::session_closed_error{ "send_request inactive" });
                }
            });
        return std::move(future);
    }
}
