#include "stdafx.h"
#include "server.hpp"

namespace net::server
{
    session<protocal::http>::session(boost::asio::ip::tcp::socket&& socket,
                                     boost::asio::io_context& context,
                                     std::filesystem::path root,
                                     bool chunked)
        : session_base(std::move(socket), context)
        , root_path_(std::move(root))
        , strand_(socket.get_executor().context()) {
        assert(socket_.is_open());
        assert(std::filesystem::is_directory(root_path_));
        static auto make_logger = core::console_logger_factory("net.server.session");
        std::tie(core::as_mutable(index_),
                 core::as_mutable(logger_)) = make_logger();
        logger_->info("socket peer endpoint {}/{}", socket_.local_endpoint(), socket_.remote_endpoint());
        logger_->info("file root path {}", root_path_);
    }

    void session<protocal::http>::wait_request() {
        logger_->info("wait_request");
        auto request_ptr = std::make_unique<request<dynamic_body>>();
        auto& request_ref = *request_ptr;
        boost::beast::http::async_read(socket_,
                                       recvbuf_,
                                       request_ref,
                                       on_recv_request(std::move(request_ptr)));
    }

    http_session_ptr session<protocal::http>::create(socket_type&& socket,
                                                     boost::asio::io_context& context,
                                                     std::filesystem::path root,
                                                     bool chunked) {
        return std::make_unique<http_session>(std::move(socket),
                                              context,
                                              std::move(root),
                                              chunked);
    }
}
