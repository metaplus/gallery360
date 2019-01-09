#include "stdafx.h"
#include "connector.hpp"

namespace net::client
{
    auto logger = core::console_logger_access("net.connector");

    connector<protocal::tcp>::connector(boost::asio::io_context& context)
        : context_{ context }
        , resolver_{ context }
        , connect_sequence_{ context.get_executor() } {}

    void connector<protocal::tcp>::fail_socket_then_cancel(boost::system::error_code errc) {
        assert(connect_sequence_.running_in_this_thread());
        logger().error("close error {} message {}", errc, errc.message());
        resolver_.cancel();
        for (auto& connect_token : connect_list_) {
            connect_token.socket.setException(std::runtime_error{ errc.message() });
        }
    }

    auto connector<protocal::tcp>::on_resolve() {
        return [this](boost::system::error_code error,
                      boost::asio::ip::tcp::resolver::results_type endpoints) {
            assert(connect_sequence_.running_in_this_thread());
            logger().info("on_resolve error {} message {}", error, error.message());
            if (error) {
                return fail_socket_then_cancel(error);
            }
            auto socket_ptr = std::make_unique<socket_type>(context_);
            auto& socket_ref = socket_ptr.operator*();
            boost::asio::async_connect(
                socket_ref, endpoints, [this,
                    socket_ptr = std::move(socket_ptr),
                    promise = std::move(connect_list_.front().socket)
                ](boost::system::error_code error,
                  boost::asio::ip::tcp::endpoint endpoint) mutable {
                    logger().info("on_connect error {} message {}", error, error.message());
                    if (error) {
                        promise.setException(std::runtime_error{ error.message() });
                        boost::asio::post(connect_sequence_, [=] {
                            fail_socket_then_cancel(error);
                        });
                        return;
                    }
                    promise.setValue(std::move(*socket_ptr));
                });
            connect_list_.pop_front();
            if (!connect_list_.empty()) {
                resolve_front_endpoint();
            }
        };
    }

    folly::SemiFuture<protocal::protocal_base<protocal::tcp>::socket_type>
    connector<protocal::tcp>::connect_socket(std::string_view host, std::string_view service) {
        auto [promise, future] = folly::makePromiseContract<socket_type>();
        boost::asio::post(
            connect_sequence_, [this, host = std::string{ host },
                service = std::string{ service }, promise = std::move(promise)]()mutable {
                connect_list_.emplace_back(
                    connect_token{ std::move(host), std::move(service), std::move(promise) });
                if (connect_list_.size() == 1) {
                    resolve_front_endpoint();
                }
            });
        return std::move(future);
    }

    void connector<protocal::tcp>::resolve_front_endpoint() {
        assert(connect_sequence_.running_in_this_thread());
        auto& connect_token = connect_list_.front();
        resolver_.async_resolve(connect_token.host, connect_token.service,
                                boost::asio::bind_executor(connect_sequence_, on_resolve()));
    }
}
