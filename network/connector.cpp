#include "stdafx.h"
#include "connector.hpp"

namespace net::client
{
    connector<protocal::tcp>::connector(boost::asio::io_context& context)
        : context_(context)
        , resolver_(context) {}

    boost::future<boost::asio::ip::tcp::socket> connector<protocal::tcp>::async_connect_socket(pending&& pending) {
        auto future_socket = pending.promise.get_future();
        {
            auto const wlock = resolve_pendlist_.wlock();
            auto const pending_iter = wlock->insert(wlock->end(), std::move(pending));
            if (wlock->size() <= 1) // the only pending work to resolve
            {
                auto const inactive = is_active(true);
                assert(!inactive);
                boost::asio::post(context_, on_establish_session(pending_iter));
            }
        }
        return future_socket;
    }

    void connector<protocal::tcp>::fail_promises_then_close_resolver(boost::system::error_code errc) {
        fmt::print(std::cerr, "resolver: close errc {}, errmsg {}\n", errc, errc.message());
        resolver_.cancel();
        for (auto& pending : *resolve_pendlist_.wlock())
            pending.promise.set_exception(std::runtime_error{ errc.message() });
        auto const active = is_active(false);
        assert(active);
    }

    folly::Function<void() const> connector<protocal::tcp>::on_establish_session(std::list<pending>::iterator pending) {
        return [this, pending] {
            resolver_.async_resolve(pending->host, pending->service, on_resolve(pending));
        };
    }

    folly::Function<void(boost::system::error_code errc, boost::asio::ip::tcp::resolver::results_type endpoints) const> connector<protocal::tcp>::on_resolve(std::list<pending>::iterator pending) {
        return [this, pending](boost::system::error_code errc, boost::asio::ip::tcp::resolver::results_type endpoints) {
            fmt::print(std::cout, "connector: on_resolve errc {}, errmsg {}\n", errc, errc.message());
            auto& promise_socket = pending->promise;
            if (errc) {
                promise_socket.set_exception(std::runtime_error{ errc.message() });
                return fail_promises_then_close_resolver(errc);
            }
            auto socket_ptr = std::make_unique<boost::asio::ip::tcp::socket>(context_);
            auto& socket_ref = *socket_ptr;
            boost::asio::async_connect(socket_ref, endpoints, on_connect(std::move(socket_ptr), std::move(promise_socket)));
            auto const wlock = resolve_pendlist_.wlock();
            wlock->erase(pending);
            if (wlock->size())
                return boost::asio::dispatch(context_, on_establish_session(wlock->begin()));
            auto const active = is_active(false);
            assert(active);
        };
    }

    folly::Function<void(boost::system::error_code errc, boost::asio::ip::tcp::endpoint endpoint)> connector<protocal::tcp>::on_connect(std::unique_ptr<boost::asio::ip::tcp::socket> socket_ptr, boost::promise<socket_type>&& promise_socket) {
        return[this, socket_ptr = std::move(socket_ptr), promise_socket = std::move(promise_socket)]
        (boost::system::error_code errc, boost::asio::ip::tcp::endpoint endpoint) mutable
        {
            fmt::print(std::cout, "connector: on_connect errc {}, errmsg {}\n", errc, errc.message());
            if (errc) {
                promise_socket.set_exception(std::runtime_error{ errc.message() });
                return fail_promises_then_close_resolver(errc);
            }
            promise_socket.set_value(std::move(*socket_ptr.release()));
        };
    }
}
