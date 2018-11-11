#include "stdafx.h"
#include "client.hpp"

namespace net::client
{
    namespace http = boost::beast::http;

    http_session::session(socket_type&& socket,
                          boost::asio::io_context& context)
        : session_base(std::move(socket), context) {
        assert(socket_.is_open());
        static auto make_logger = core::index_logger_factory("net.client.session");
        std::tie(core::as_mutable(index_),
                 core::as_mutable(logger_)) = make_logger();
        logger_->info("constructor socket endpoint client {} server {}", socket_.local_endpoint(), socket_.remote_endpoint());
        reserve_recvbuf_capacity();
    }

    http_session_ptr session<protocal::http>::create(socket_type&& socket,
                                                     boost::asio::io_context& context) {
        return std::make_unique<http_session>(std::move(socket), context);
    }

    void http_session::config_response_parser() {
        response_parser_.emplace();
        response_parser_->body_limit(std::numeric_limits<uint64_t>::max());
    }

    folly::Function<void(boost::system::error_code, std::size_t)>
    session<protocal::http>::on_send_request(std::any&& request) {
        return [this, request = std::move(request)](boost::system::error_code errc,
                                                    std::size_t transfer_size) mutable {
            logger_->info("on_send_request errc {} transfer {}", errc, transfer_size);
            if (errc) {
                logger_->error("on_send_request failure");
                return shutdown_and_reject_request(core::bad_request_error{ errc.message() },
                                                   errc,
                                                   boost::asio::socket_base::shutdown_send);
            }
            http::async_read(socket_, recvbuf_, *response_parser_, on_recv_response());
        };
    }

    folly::Function<void(boost::system::error_code, std::size_t)>
    session<protocal::http>::on_recv_response() {
        return [this](boost::system::error_code errc, std::size_t transfer_size) mutable {
            logger_->info("on_recv_response errc {} transfer {}", errc, transfer_size);
            if (errc) {
                logger_->error("on_recv_response failure");
                return shutdown_and_reject_request(core::bad_response_error{ errc.message() },
                                                   errc,
                                                   boost::asio::socket_base::shutdown_receive);
            }
            if (response_parser_->get().result() != http::status::ok) {
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
}
