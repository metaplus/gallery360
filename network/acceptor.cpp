#include "stdafx.h"
#include "acceptor.hpp"

namespace net::server
{
    auto logger = core::console_logger_access("net.acceptor");

    acceptor<boost::asio::ip::tcp>::acceptor(boost::asio::ip::tcp::endpoint endpoint,
                                             boost::asio::io_context& context, bool reuse_addr)
        : context_{ context }
        , acceptor_{ context, endpoint, reuse_addr } {
        assert(acceptor_.is_open());
        logger().info("listen address {}, port {}", endpoint.address(), listen_port());
    }

    acceptor<boost::asio::ip::tcp>::acceptor(uint16_t port,
                                             boost::asio::io_context& context, bool reuse_addr)
        : acceptor{
            boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4(), port },
            context, reuse_addr
        } {}

    uint16_t acceptor<boost::asio::ip::tcp>::listen_port() const {
        return acceptor_.local_endpoint().port();
    }

    auto acceptor<boost::asio::ip::tcp>::on_accept(folly::Promise<socket_type>&& promise) {
        return [this, promise = std::move(promise)](boost::system::error_code error,
                                                    boost::asio::ip::tcp::socket socket) mutable {
            logger().info("on_accept error {}, message {}", error, error.message());
            if (error) {
                promise.setException(std::runtime_error{ "acceptor error" });
                return close(error);
            }
            logger().info("on_accept local {} remote {}", socket.local_endpoint(), socket.remote_endpoint());
            promise.setValue(std::move(socket));
        };
    }

    auto acceptor<boost::asio::ip::tcp>::accept_socket() -> folly::SemiFuture<socket_type> {
        auto [promise, future] = folly::makePromiseContract<socket_type>();
        acceptor_.async_accept(on_accept(std::move(promise)));
        return std::move(future);
    }

    void acceptor<boost::asio::ip::tcp>::close(std::optional<boost::system::error_code> error,
                                               bool cancel) {
        if (error.has_value()) {
            logger().error("close error {} message {}", *error, error->message());
        }
        if (cancel) {
            return acceptor_.cancel();
        }
        acceptor_.close();
    }
}
