#pragma once

namespace net
{
    template<typename Protocal>
    class connector;

    template<>
    class connector<boost::asio::ip::tcp> : detail::state_base
    {
        using protocal_type = boost::asio::ip::tcp;
        using socket_type = boost::asio::ip::tcp::socket;

        struct pending_entry
        {
            std::string_view host;
            std::string_view service;
            boost::promise<socket_type> promise;
        };

        folly::Synchronized<std::list<pending_entry>> resolve_pendlist_;
        boost::asio::io_context& context_;
        boost::asio::ip::tcp::resolver resolver_;

    public:
        explicit connector(boost::asio::io_context& context)
            : context_(context)
            , resolver_(context)
        {}

        template<typename Protocal, typename ...SessionArgs>
        std::unique_ptr<client::session<Protocal>>
            establish_session(std::string_view host, std::string_view service, SessionArgs&& ...args)
        {
            boost::promise<boost::asio::ip::tcp::socket> socket_promise;
            auto socket_future = socket_promise.get_future();
            {
                auto const wlocked_pendlist = resolve_pendlist_.wlock();
                auto const pend_iter = wlocked_pendlist->insert(
                    wlocked_pendlist->end(), pending_entry{ host, service, std::move(socket_promise) });
                if (wlocked_pendlist->size() <= 1) // the only pending work to resolve
                {
                    auto const inactive = is_active(true);
                    assert(!inactive);
                    boost::asio::post(context_, on_establish_session(pend_iter));
                }
            }
            return std::make_unique<client::session<Protocal>>(
                socket_future.get(), resolver_.get_executor().context(), std::forward<SessionArgs>(args)...);
        }

    private:
        folly::Function<void() const>
            on_establish_session(std::list<pending_entry>::iterator pend_iter)
        {
            return [this, pend_iter]
            {
                resolver_.async_resolve(pend_iter->host, pend_iter->service, on_resolve(pend_iter));
            };
        }

        folly::Function<void(boost::system::error_code errc, boost::asio::ip::tcp::resolver::results_type endpoints) const>
            on_resolve(std::list<pending_entry>::iterator pend_iter)
        {
            return[this, pend_iter](boost::system::error_code errc, boost::asio::ip::tcp::resolver::results_type endpoints)
            {
                fmt::print(std::cout, "connector: on_resolve errc {}, errmsg {}\n", errc, errc.message());
                auto& socket_promise = pend_iter->promise;
                if (errc)
                {

                    socket_promise.set_exception(std::runtime_error{ errc.message() });
                    return fail_promises_then_close_resolver(errc);
                }
                auto socket_ptr = std::make_unique<boost::asio::ip::tcp::socket>(context_);
                auto& socket_ref = *socket_ptr;
                boost::asio::async_connect(socket_ref, endpoints, on_connect(std::move(socket_ptr), std::move(socket_promise)));
                auto const wlocked_pendlist = resolve_pendlist_.wlock();
                wlocked_pendlist->erase(pend_iter);
                if (wlocked_pendlist->size())
                    return boost::asio::dispatch(context_, on_establish_session(wlocked_pendlist->begin()));
                auto const active = is_active(false);
                assert(active);
            };
        }

        folly::Function<void(boost::system::error_code errc, boost::asio::ip::tcp::endpoint endpoint)>
            on_connect(std::unique_ptr<boost::asio::ip::tcp::socket> socket_ptr,
                       boost::promise<socket_type>&& socket_promise)
        {
            return[this, socket_ptr = std::move(socket_ptr), socket_promise = std::move(socket_promise)]
            (boost::system::error_code errc, boost::asio::ip::tcp::endpoint endpoint) mutable
            {
                fmt::print(std::cout, "connector: on_connect errc {}, errmsg {}\n", errc, errc.message());
                if (errc)
                {
                    socket_promise.set_exception(std::runtime_error{ errc.message() });
                    return fail_promises_then_close_resolver(errc);
                }
                socket_promise.set_value(std::move(*socket_ptr.release()));
            };
        }

        void fail_promises_then_close_resolver(boost::system::error_code errc)
        {
            fmt::print(std::cerr, "resolver: close errc {}, errmsg {}\n", errc, errc.message());
            resolver_.cancel();
            for (auto& pend_entry : *resolve_pendlist_.wlock())
                pend_entry.promise.set_exception(std::runtime_error{ errc.message() });
            auto const active = is_active(false);
            assert(active);
        }
    };

    template class connector<boost::asio::ip::tcp>;
}
