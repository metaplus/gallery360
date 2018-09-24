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

#if 0
    folly::Function<multi_buffer()>
        http_session::transform_buffer_stream(folly::Function<std::string()> url_supplier) {
        struct stream : std::enable_shared_from_this<stream>
        {
            using future_response = std::variant<boost::future<response>, folly::SemiFuture<response>>;

            int64_t recv_count = 0;
            future_response response;
            folly::Function<future_response()> response_supplier;

            [[maybe_unused]] multi_buffer iterate_buffer() {
                //auto available_buffer = response.get().body();
                multi_buffer available_buffer;
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
#endif

    http_session_ptr session<protocal::http>::create(socket_type&& socket, boost::asio::io_context& context) {
        return std::make_unique<http_session>(std::move(socket), context);
    }

    void http_session::config_response_parser() {
        response_parser_.emplace();
        response_parser_->body_limit(std::numeric_limits<uint64_t>::max());
    }

    folly::Function<void(boost::system::error_code, std::size_t)>
        http_session::on_recv_response() {
        return [this](boost::system::error_code errc, std::size_t transfer_size) mutable {
            logger_->info("on_recv_response errc {} transfer {}", errc, transfer_size);
            if (errc) {
                logger_->error("on_recv_response failure");
                return close_promise_and_socket(response_, errc, boost::asio::socket_base::shutdown_receive);
            }
            core::visit(response_,
                        [this](folly::Promise<response>& promise) { promise.setValue(response_parser_->release()); },
                        [this](boost::promise<response>& promise) { promise.set_value(response_parser_->release()); });
        };
    }
}
