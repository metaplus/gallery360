#include "stdafx.h"
#include "acceptor.hpp"

namespace net::server
{
    auto logger = core::console_logger_access("net.acceptor");

    acceptor<boost::asio::ip::tcp>::acceptor(boost::asio::ip::tcp::endpoint endpoint,
                                             boost::asio::io_context& context,
                                             bool reuse_addr)
        : context_(context)
        , acceptor_(context, endpoint, reuse_addr) {
        core::verify(acceptor_.is_open());
        logger()->info("listen address {}, port {}", endpoint.address(), listen_port());
    }

    acceptor<boost::asio::ip::tcp>::acceptor(uint16_t port,
                                             boost::asio::io_context& context,
                                             bool reuse_addr)
        : acceptor(boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4(), port },
                   context,
                   reuse_addr) {}

    uint16_t acceptor<boost::asio::ip::tcp>::listen_port() const {
        return acceptor_.local_endpoint().port();
    }

    auto acceptor<boost::asio::ip::tcp>::accept_socket() -> folly::SemiFuture<socket_type> {
        auto [promise_socket, future_socket] = folly::makePromiseContract<socket_type>();
        accept_list_.withWLock([this, &promise_socket](std::list<acceptor::entry>& accept_list) {
            accept_list.emplace_back(std::move(promise_socket));
            if (accept_list.size() <= 1) {
                boost::asio::post(context_, [this] {
                    acceptor_.async_accept(on_accept());
                });
            }
        });
        return std::move(future_socket);
    }

    folly::Function<void(boost::system::error_code errc,
                         boost::asio::ip::tcp::socket socket)>
    acceptor<boost::asio::ip::tcp>::on_accept() {
        return [this](boost::system::error_code errc,
                      boost::asio::ip::tcp::socket socket) {
            logger()->info("on_accept errc {}, errmsg {}", errc, errc.message());
            accept_list_.withWLock(
                [this, &errc, &socket](std::list<entry>& accept_list) {
                    if (errc) {
                        for (auto& entry : accept_list)
                            entry.setException(std::runtime_error{ "acceptor error" });
                        accept_list.clear();
                        return close_acceptor(errc);
                    }
                    if (accept_list.size() > 1) {
                        boost::asio::post(context_, [this] {
                            acceptor_.async_accept(on_accept());
                        });
                    }
                    logger()->info("on_accept local {} remote {}", socket.local_endpoint(), socket.remote_endpoint());
                    accept_list.front().setValue(std::move(socket));
                    accept_list.pop_front();
                });

        };
    }

    void acceptor<boost::asio::ip::tcp>::close_acceptor(boost::system::error_code errc) {
        logger()->error("close errc {} errmsg {}", errc, errc.message());
        acceptor_.cancel();
        acceptor_.close();
    }
}
