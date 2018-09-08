#include "stdafx.h"
#include "client.hpp"

namespace detail
{
    std::atomic<int16_t> index_generator_ = 0;
}

namespace net::client
{
    namespace http = boost::beast::http;

    int16_t info::net_session_index() {
        static std::atomic<int16_t> index_generator = 0;
        return index_generator.fetch_add(1);
    }

    std::shared_ptr<spdlog::logger> info::create_logger(int16_t index) {
        return spdlog::stdout_color_mt(fmt::format("{}${}", "net.client.session", index));
    }

    http_session::session(socket_type&& socket, boost::asio::io_context& context)
        : session_base(std::move(socket), context) {
        assert(socket_.is_open());
        logger_->info("constructor socket endpoint client {} server {}", socket_.local_endpoint(), socket_.remote_endpoint());
        reserve_recvbuf_capacity();
    }

    folly::Function<multi_buffer()>
        http_session::transform_buffer_stream(folly::Function<std::string()> url_supplier) {
        struct stream : std::enable_shared_from_this<stream>
        {
            using future_response = boost::future<response>;

            int64_t recv_count = 0;
            future_response response;
            folly::Function<future_response()> response_supplier;

            [[maybe_unused]] multi_buffer iterate_buffer() {
                auto available_buffer = response.get().body();
                ++recv_count;
                response = response_supplier();
                return available_buffer;
            }

            decltype(auto) drain_error(std::string&& message) {
                struct stream_drain_error : std::runtime_error
                {
                    std::shared_ptr<stream> stream_ptr;
                    stream_drain_error(const std::string& message, std::shared_ptr<stream> stream)
                        : runtime_error(message), stream_ptr(stream) {}
                };
                return stream_drain_error{ message,shared_from_this() };
            }
        };
        const auto stream_ptr = std::make_shared<stream>();
        stream_ptr->response_supplier = [this, url_supplier = std::move(url_supplier)]{
            folly::Uri uri{ std::invoke(core::as_mutable(url_supplier)) };
            return async_send_request(net::make_http_request<empty_body>(uri.host(), uri.path()));
        };
        stream_ptr->iterate_buffer();
        return[this, stream_ptr] {
            try {
                return stream_ptr->iterate_buffer();
            } catch (...) {
                throw stream_ptr->drain_error(boost::current_exception_diagnostic_information());
            }
        };
    }

    boost::promise<session<protocal::http>::response>
        http_session::prepare_response_parser() {
        response_parser_.emplace();
        response_parser_->body_limit(std::numeric_limits<uint64_t>::max());
        return boost::promise<response>{};
    }

    folly::Function<void(boost::system::error_code, std::size_t)>
        http_session::on_send_request(request_ptr<empty_body> request) {
        return[this, request = std::move(request)](boost::system::error_code errc, std::size_t transfer_size)
        {
            logger_->info("on_send_request errc {} transfer {}", errc, transfer_size);
            if (errc) {
                logger_->error("on_send_request failure");
                return close_promise_and_socket(promise_response_, errc, boost::asio::socket_base::shutdown_send);
            }
            if (is_chunked())
                throw core::not_implemented_error{ __FUNCSIG__ };
            http::async_read(socket_, recvbuf_, *response_parser_, on_recv_response());
        };
    }

    folly::Function<void(boost::system::error_code, std::size_t)>
        http_session::on_recv_response() {
        return [this](boost::system::error_code errc, std::size_t transfer_size) mutable {
            logger_->info("on_recv_response errc {} transfer {}", errc, transfer_size);
            if (errc) {
                logger_->error("on_recv_response, failure");
                return close_promise_and_socket(promise_response_, errc, boost::asio::socket_base::shutdown_receive);
            }
            promise_response_.set_value(response_parser_->release());
        };
    }
}
