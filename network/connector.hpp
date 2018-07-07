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

        enum pending_entry_index : size_t { host, service, socket, entry_size };

        folly::Synchronized<std::list<
            boost::hana::tuple<std::string_view, std::string_view, boost::promise<socket_type>>
        >> resolve_pendlist_;
        boost::asio::io_context& context_;
        boost::asio::ip::tcp::resolver resolver_;

    public:
        explicit connector(boost::asio::io_context& context)
            : context_(context)
            , resolver_(context) {}

        template<typename Protocal, typename ...SessionArgs>
        std::shared_ptr<client::session<Protocal>>
        establish_session(std::string_view host, std::string_view service, SessionArgs&& ...args)
        {
            boost::promise<boost::asio::ip::tcp::socket> socket_promise;
            auto socket_future = socket_promise.get_future();
            {
                auto const wlocked_pendlist = resolve_pendlist_.wlock();
                auto const pendlist_iter = wlocked_pendlist->insert(
                    wlocked_pendlist->end(), boost::hana::make_tuple(host, service, std::move(socket_promise)));
                if (wlocked_pendlist->size() <= 1) // the only pending work to resolve
                {
                    auto const inactive = is_active(true);
                    assert(!inactive);
                    boost::asio::post(context_, on_establish_session(pendlist_iter));
                }
            }
            return std::make_shared<client::session<Protocal>>(
                socket_future.get(), resolver_.get_executor().context(), std::forward<SessionArgs>(args)...);
        }

    private:
        folly::Function<void() const> on_establish_session(decltype(resolve_pendlist_)::DataType::iterator pendlist_iter)
        {
            using boost::hana::size_c;
            return [this, pendlist_iter]
            {
                auto& pending = *pendlist_iter;
                resolver_.async_resolve(pending[size_c<host>], pending[size_c<service>], on_resolve(pendlist_iter));
            };
        }

        folly::Function<void(boost::system::error_code errc, boost::asio::ip::tcp::resolver::results_type endpoints)>
        on_resolve(decltype(resolve_pendlist_)::DataType::iterator pendlist_iter)
        {
            using boost::hana::size_c;
            auto& pending = *pendlist_iter;
            return
                [this, pendlist_iter, socket_promise = std::move(pending[size_c<socket>])
                ](boost::system::error_code errc, boost::asio::ip::tcp::resolver::results_type endpoints) mutable
            {
                fmt::print(std::cout, "connector: handle resolve errc {}, errmsg {}\n", errc, errc.message());
                if (errc)
                {
                    socket_promise.set_exception(boost::make_exceptional(std::runtime_error{ errc.message() }));
                    return close_resolver(errc);
                }
                auto socket_ptr = std::make_unique<boost::asio::ip::tcp::socket>(context_);
                auto& socket_ref = *socket_ptr;
                boost::asio::async_connect(
                    socket_ref, endpoints, on_connect(std::move(socket_ptr), std::move(socket_promise)));
                auto const wlocked_pendlist = resolve_pendlist_.wlock();
                wlocked_pendlist->erase(pendlist_iter);
                if (wlocked_pendlist->empty())
                {
                    auto const active = is_active(false);
                    assert(active);
                }
                boost::asio::dispatch(context_, on_establish_session(wlocked_pendlist->begin()));
            };
        }

        folly::Function<void(boost::system::error_code errc, boost::asio::ip::tcp::endpoint endpoint)>
        on_connect(std::unique_ptr<boost::asio::ip::tcp::socket> socket_ptr,
                   boost::promise<socket_type>&& socket_promise)
        {
            return
                [this, socket_ptr = std::move(socket_ptr), socket_promise = std::move(socket_promise)
                ](boost::system::error_code errc, boost::asio::ip::tcp::endpoint endpoint) mutable
            {
                fmt::print(std::cout, "connector: on_connect errc {}, errmsg {}\n", errc, errc.message());
                if (errc)
                {
                    socket_promise.set_exception(std::runtime_error{ errc.message() });
                    return close_resolver(errc);
                }
                socket_promise.set_value(std::move(*socket_ptr.release()));
            };
        }

        void close_resolver(boost::system::error_code errc)
        {
            fmt::print(std::cerr, "resolver: close errc {}, errmsg {}\n", errc, errc.message());
            resolver_.cancel();
            using boost::hana::size_c;
            for (auto& tuple : *resolve_pendlist_.wlock())
                tuple[size_c<socket>].set_exception(std::runtime_error{ errc.message() });
            auto const active = is_active(false);
            assert(active);
        }
    };

    template class connector<boost::asio::ip::tcp>;
}
