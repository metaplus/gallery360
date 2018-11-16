#include "stdafx.h"
#include "server.hpp"

namespace net::server
{
    auto make_logger = core::console_logger_factory("net.server.session");

    session<protocal::http>::session(boost::asio::ip::tcp::socket&& socket,
                                     boost::asio::io_context& context,
                                     std::filesystem::path root,
                                     folly::Function<void() const> error_callback)
        : session_base(std::move(socket), context)
        , root_path_(std::move(root))
        , error_callback_{ std::move(error_callback) }
        , strand_(socket.get_executor().context()) {
        assert(socket_.is_open());
        assert(std::filesystem::is_directory(root_path_));
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
                                                     folly::Function<void() const> error_callback) {
        return std::make_unique<http_session>(std::move(socket),
                                              context,
                                              std::move(root),
                                              std::move(error_callback));
    }

    std::filesystem::path session<protocal::http>::concat_target_path(boost::beast::string_view request_target) const {
        return std::filesystem::path{ root_path_ }
            .concat(request_target.begin(), request_target.end());
    }

    file_body::value_type session<protocal::http>::file_response_body(std::filesystem::path& target) {
        boost::system::error_code file_errc;
        file_body::value_type response_body;
        response_body.open(target.generic_string().c_str(),
                           boost::beast::file_mode::scan,
                           file_errc);
        assert(!file_errc);
        return response_body;
    }

    void session<protocal::http>::close_socket_then_callback(boost::system::error_code errc,
                                                             boost::asio::socket_base::shutdown_type shutdown_type) {
        std::string_view shutdown_name;
        switch (shutdown_type) {
            case boost::asio::socket_base::shutdown_receive: shutdown_name = "shutdown_receive"sv;
                break;
            case boost::asio::socket_base::shutdown_send: shutdown_name = "shutdown_send"sv;
                break;
            case boost::asio::socket_base::shutdown_both: shutdown_name = "shutdown_both"sv;
                break;

        }
        assert(!std::empty(shutdown_name));
        logger_->error("close_socket {}", shutdown_name.data());
        logger_->error("error message {}", errc ? errc.message() : "null");
        close_socket(shutdown_type);
        error_callback();
    }

    void session<protocal::http>::error_callback() const {
        if (error_callback_) {
            logger_->error("error callback execute");
            error_callback_();
        } else {
            logger_->warn("error callback null");
        }
    }
}
