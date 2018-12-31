#include "stdafx.h"
#include "connector.hpp"

namespace net::client
{
    auto logger = core::console_logger_access("net.connector");

    connector<protocal::tcp>::connector(boost::asio::io_context& context)
        : context_(context)
        , resolver_(context) {}

    void connector<protocal::tcp>::shutdown_and_reject_request(boost::system::error_code errc) {
        logger()->error("close errc {} errmsg {}", errc, errc.message());
        resolver_.cancel();
        for (auto& entry : *resolve_list_.wlock()) {
            entry.socket.setException(std::runtime_error{ errc.message() });
        }
    }

    folly::Function<void(boost::system::error_code errc,
                         boost::asio::ip::tcp::resolver::results_type endpoints) const>
    connector<protocal::tcp>::on_resolve(std::list<entry>::iterator entry_iter) {
        return [this, entry_iter](boost::system::error_code errc,
                                  boost::asio::ip::tcp::resolver::results_type endpoints) {
            logger()->info("on_resolve errc {} errmsg {}", errc, errc.message());
            if (errc) {
                entry_iter->socket.setException(std::runtime_error{ errc.message() });
                return shutdown_and_reject_request(errc);
            }
            auto socket_ptr = std::make_unique<socket_type>(context_);
            auto& socket_ref = *socket_ptr;
            boost::asio::async_connect(
                socket_ref, endpoints,
                [this, socket_ptr = std::move(socket_ptr), entry = std::move(*entry_iter)](boost::system::error_code errc,
                                                                                           boost::asio::ip::tcp::endpoint endpoint) mutable {
                    logger()->info("on_connect errc {} errmsg {}", errc, errc.message());
                    if (errc) {
                        entry.socket.setException(std::runtime_error{ errc.message() });
                        return shutdown_and_reject_request(errc);
                    }
                    entry.socket.setValue(std::move(*socket_ptr));
                });
            resolve_list_.withWLock(
                [this, entry_iter](std::list<entry>& resolve_list) {
                    resolve_list.erase(entry_iter);
                    if (resolve_list.size()) {
                        return boost::asio::dispatch(context_, [this, entry_iterator = resolve_list.begin()] {
                            resolver_.async_resolve(entry_iterator->host,
                                                    entry_iterator->service,
                                                    on_resolve(entry_iterator));
                        });
                    }
                });
        };
    }
}
