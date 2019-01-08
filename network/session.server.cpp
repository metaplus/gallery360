#include "stdafx.h"
#include "session.server.hpp"

namespace net::server
{
    namespace http = boost::beast::http;

    auto make_logger = core::console_logger_factory("net.server.session");

    session<protocal::http>::session(boost::asio::ip::tcp::socket&& socket,
                                     boost::asio::io_context& context)
        : session_base{ std::move(socket), context } {
        assert(socket_.is_open());
        std::tie(core::as_mutable(index_),
                 core::as_mutable(logger_)) = make_logger();
        core::as_mutable(identity_) = fmt::format("session${}", index_);
        logger_->info("socket peer endpoint {}/{}", socket_.local_endpoint(), socket_.remote_endpoint());
        logger_->info("file root path {}", root_path_);
    }

    session<protocal::http>& session<protocal::http>::root_directory(std::filesystem::path root) {
        assert(std::filesystem::is_directory(root));
        root_path_ = std::move(root);
        return *this;
    }

    folly::SemiFuture<folly::Unit> session<protocal::http>::process_requests() {
        auto completion = completion_.getSemiFuture();
        receive_request();
        return completion;
    }

    auto session<protocal::http>::create(socket_type&& socket,
                                         boost::asio::io_context& context,
                                         std::filesystem::path root) -> pointer {
        auto instance = std::make_unique<session>(std::move(socket), context);
        instance->root_directory(std::move(root));
        return instance;
    }

    auto session<protocal::http>::on_recv_request(request_ptr<dynamic_body> request) {
        return [this, request = std::move(request)](boost::system::error_code errc,
                                                    std::size_t transfer_size) {
            logger_->info("on_recv_request errc {} transfer {}", errc, transfer_size);
            logger_->debug("on_recv_request request head {}", request->base());
            if (errc || request->need_eof()) {
                return close_socket_then_complete(errc, boost::asio::socket_base::shutdown_receive);
            }
            auto target_path = concat_target_path(request->target());
            logger_->info("on_recv_request {} {}", target_path, exists(target_path) ? "valid" : "invalid");
            const auto send_response = [this](auto&& response_ptr) {
                auto& response_ref = *response_ptr;
                logger_->info("on_recv_request response reason {}", response_ptr->reason());
                http::async_write(socket_, response_ref, on_send_response(std::move(response_ptr)));
            };
            if (std::filesystem::exists(target_path)) {
                auto response_body = file_response_body(target_path);
                auto response = std::make_unique<
                    http::response<file_body>>(http::status::ok, request->version(),
                                               std::move(response_body));
                response->content_length(response_body.size());
                response->set(http::field::server, "MetaPlus");
                response->keep_alive(request->keep_alive());
                send_response(std::move(response));
            } else {
                logger_->error("on_recv_request target non-exist");
                send_response(std::make_unique<
                    http::response<empty_body>>(http::status::bad_request, request->version()));
            }
        };
    }

    void session<protocal::http>::receive_request() {
        logger_->info("receive_request");
        auto request_ptr = std::make_unique<request<dynamic_body>>();
        auto& request_ref = *request_ptr;
        http::async_read(socket_, recvbuf_, request_ref,
                         on_recv_request(std::move(request_ptr)));
    }

    std::filesystem::path session<protocal::http>::concat_target_path(boost::beast::string_view request_target) const {
        return std::filesystem::path{ root_path_ }
            .concat(request_target.begin(), request_target.end());
    }

    file_body::value_type session<protocal::http>::file_response_body(std::filesystem::path& target) {
        boost::system::error_code file_error;
        file_body::value_type response_body;
        response_body.open(target.generic_string().c_str(),
                           boost::beast::file_mode::scan, file_error);
        assert(!file_error);
        return response_body;
    }

    void session<protocal::http>::close_socket_then_complete(boost::system::error_code errc,
                                                             boost::asio::socket_base::shutdown_type shutdown_type) {
        completion_.setWith([=] {
            std::string_view shutdown_name;
            switch (shutdown_type) {
                case boost::asio::socket_base::shutdown_receive:
                    shutdown_name = "shutdown_receive"sv;
                    break;
                case boost::asio::socket_base::shutdown_send:
                    shutdown_name = "shutdown_send"sv;
                    break;
                case boost::asio::socket_base::shutdown_both:
                    shutdown_name = "shutdown_both"sv;
                    break;
            }
            assert(!std::empty(shutdown_name));
            logger_->error("close_socket {} error message {}", shutdown_name.data(), errc ? errc.message() : "null");
            close_socket(shutdown_type);
        });
    }
}
