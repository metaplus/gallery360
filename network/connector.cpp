#include "stdafx.h"
#include "connector.hpp"

namespace net::client
{
    const auto logger = spdlog::stdout_color_mt("net.connector");

    void connector<protocal::tcp>::pending::set_exception(boost::system::error_code errc) {
        std::visit(
            core::overloaded{
                [errc](folly::Promise<socket_type>& promise) { promise.setException(std::runtime_error{ errc.message() }); },
                [errc](boost::promise<socket_type>& promise) { promise.set_exception(std::runtime_error{ errc.message() }); }
            },
            socket);
    }

    void connector<protocal::tcp>::pending::set_socket(socket_type& socket) {
        std::visit(
            core::overloaded{
                [&socket](folly::Promise<socket_type>& promise) { promise.setValue(std::move(socket)); },
                [&socket](boost::promise<socket_type>& promise) { promise.set_value(std::move(socket)); }
            },
            this->socket);
    }

    connector<protocal::tcp>::connector(boost::asio::io_context& context)
        : context_(context)
        , resolver_(context) {}

    void connector<protocal::tcp>::insert_pendlist(pending&& pending) {
        auto const wlock = resolve_pendlist_.wlock();
        auto const pending_iter = wlock->insert(wlock->end(), std::move(pending));
        if (wlock->size() <= 1) // the only pending work to resolve
        {
            auto const inactive = is_active(true);
            assert(!inactive);
            boost::asio::post(context_, on_establish_session(pending_iter));
        }
    }

    void connector<protocal::tcp>::close_promises_and_resolver(boost::system::error_code errc) {
        logger->error("close errc {} errmsg {}", errc, errc.message());
        resolver_.cancel();
        for (auto& pending : *resolve_pendlist_.wlock()) {
            pending.set_exception(errc);
        }
        auto const active = is_active(false);
        assert(active);
    }

    folly::Function<void() const>
        connector<protocal::tcp>::on_establish_session(std::list<pending>::iterator pending) {
        return [this, pending] {
            resolver_.async_resolve(pending->host, pending->service, on_resolve(pending));
        };
    }

    folly::Function<void(boost::system::error_code errc, boost::asio::ip::tcp::resolver::results_type endpoints) const>
        connector<protocal::tcp>::on_resolve(std::list<pending>::iterator pending) {
        return [this, pending](boost::system::error_code errc, boost::asio::ip::tcp::resolver::results_type endpoints) {
            logger->info("on_resolve errc {} errmsg {}", errc, errc.message());
            if (errc) {
                pending->set_exception(errc);
                return close_promises_and_resolver(errc);
            }
            auto socket = folly::makeMoveWrapper(std::make_unique<socket_type>(context_));
            auto& socket_ref = **socket;
            boost::asio::async_connect(
                socket_ref, endpoints,
                [this, socket, pending](boost::system::error_code errc, boost::asio::ip::tcp::endpoint endpoint) {
                    logger->info("on_connect errc {} errmsg {}", errc, errc.message());
                    if (errc) {
                        pending->set_exception(errc);
                        return close_promises_and_resolver(errc);
                    }
                    pending->set_socket(**socket);
                });
            auto const wlock = resolve_pendlist_.wlock();
            wlock->erase(pending);
            if (wlock->size()) {
                return boost::asio::dispatch(context_, on_establish_session(wlock->begin()));
            }
            auto const active = is_active(false);
            assert(active);
        };
    }
}
