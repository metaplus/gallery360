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
}
