#include "stdafx.h"
#include "session.client.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/null_sink.h>
#include <boost/asio/bind_executor.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/write.hpp>
#include <fmt/ostream.h>

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
        core::as_mutable(tracer_) = spdlog::create<spdlog::sinks::null_sink_st>(identity_);
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
                return fail_request_then_close(
                    core::bad_response_error{} << core::errinfo_code{ errc },
                    errc, boost::asio::socket_base::shutdown_receive);
            }
            if (response_parser_->get().result() != http::status::ok) {
                logger_().error("on_recv_response bad response");
                return fail_request_then_close(
                    core::bad_response_error{} << core::errinfo_message{
                        response_parser_->get().reason().to_string()
                    },
                    errc, boost::asio::socket_base::shutdown_receive);
            }
            tracer_->info("response=recv:index={}:transfer={}", index, transfer_size);
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
                return fail_request_then_close(
                    core::bad_request_error{} << core::errinfo_code{ errc },
                    errc, boost::asio::socket_base::shutdown_send);
            }
            tracer_->info("request=send:index={}:transfer={}", index, transfer_size);
            http::async_read(socket_, recvbuf_, *response_parser_,
                             boost::asio::bind_executor(request_sequence_, on_recv_response(index)));
        };
    }

    void session<protocal::http>::send_front_request() {
        assert(request_sequence_.running_in_this_thread());
        emplace_response_parser();
        auto request_index = ++round_index_;
        tracer_->info("request=ready:index={}", request_index);
        http::async_write(socket_, request_list_.front().first,
                          boost::asio::bind_executor(request_sequence_, on_send_request(request_index)));
    }

    void session<protocal::http>::trace_by(spdlog::sink_ptr sink) const {
        spdlog::drop(identity_);
        core::as_mutable(tracer_) = core::make_async_logger(identity_, std::move(sink));
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
                    response.setException(core::session_closed_error{});
                }
            });
        return std::move(future);
    }
}
