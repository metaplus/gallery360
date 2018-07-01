#pragma once
#include <boost/container_hash/extensions.hpp>

namespace net::client
{
    template<typename Protocal>
    class connector;

    template<>
    class connector<boost::asio::ip::tcp>
    {
        enum state_index : size_t { active, state_size };

        enum pending_entry_index : size_t { host, service, socket, entry_size };

        using protocal_type = boost::asio::ip::tcp;
        using socket_type = boost::asio::ip::tcp::socket;

        folly::AtomicBitSet<state_size> mutable state_;
        folly::Synchronized<std::list<
            boost::hana::tuple<std::string_view, std::string_view, boost::promise<socket_type>>
        >> resolve_pendlist_;
        boost::asio::io_context& context_;
        boost::asio::ip::tcp::resolver resolver_;
        std::map<
            boost::hana::pair<std::string_view, std::string_view>,
            boost::asio::ip::tcp::resolver::results_type
        > endpoint_cache_;

    public:
        explicit connector(boost::asio::io_context& context)
            : context_(context)
            , resolver_(context) {}

        template<typename ApplicationProtocal, typename ...SessionArgs>
        std::shared_ptr<session<ApplicationProtocal, boost::asio::ip::tcp::socket>>
        establish_session(std::string_view host, std::string_view service, SessionArgs&& ...args)
        {
            boost::promise<boost::asio::ip::tcp::socket> socket_promise;
            auto socket_future = socket_promise.get_future();
            {
                auto const wlocked_pendlist = resolve_pendlist_.wlock();
                auto const pendlist_iter = wlocked_pendlist->insert(
                    wlocked_pendlist->end(), boost::hana::make_tuple(host, service, std::move(socket_promise)));
                if (wlocked_pendlist->size() <= 1) // the only pending work to resolve
                    boost::asio::post(context_, [this, pendlist_iter]
                    {
                        auto const old_active_state = state_.set(active, std::memory_order_release);
                        assert(!old_active_state);
                        exec_resolve(pendlist_iter);
                    });
            }
            return std::make_shared<session<ApplicationProtocal, boost::asio::ip::tcp::socket>>(
                socket_future.get(), resolver_.get_executor().context(), std::forward<SessionArgs>(args)...);
        }

    private:
        void exec_resolve(decltype(resolve_pendlist_)::DataType::iterator pendlist_iter)
        {
            using boost::hana::size_c;
            auto& pending = *pendlist_iter;
            auto const cached_endpoints_iter = endpoint_cache_.find(
                boost::hana::make_pair(pending[size_c<host>], pending[size_c<service>]));
            if (cached_endpoints_iter != endpoint_cache_.end())
            {
                auto socket_promise = std::move(pending[size_c<socket>]);
                exec_connect(cached_endpoints_iter->second, std::move(socket_promise));
                return erase_pendlist_then_resolve(pendlist_iter);
            }
            resolver_.async_resolve(
                pending[size_c<host>], pending[size_c<service>],
                [this, pendlist_iter, socket_promise = std::move(pending[size_c<socket>])
                ](boost::system::error_code errc, boost::asio::ip::tcp::resolver::results_type endpoints) mutable
                {
                    fmt::print(std::cout, "connector: handle resolve errc {}, errmsg {}\n", errc, errc.message());
                    if (errc)
                    {
                        socket_promise.set_exception(boost::make_exceptional(std::runtime_error{ errc.message() }));
                        return close_resolver(errc);
                    }
                    auto& pending = *pendlist_iter;
                    auto const [emplaced_endpoints_iter, success] = endpoint_cache_.emplace(
                        boost::hana::make_pair(pending[size_c<host>], pending[size_c<service>]), endpoints);
                    assert(success);
                    exec_connect(emplaced_endpoints_iter->second, std::move(socket_promise));
                    erase_pendlist_then_resolve(pendlist_iter);
                }
            );
        }

        void exec_connect(decltype(endpoint_cache_)::mapped_type const& endpoints,
                          boost::promise<socket_type>&& socket_promise)
        {
            auto socket_ptr = std::make_unique<boost::asio::ip::tcp::socket>(context_);
            auto& socket_ref = *socket_ptr;
            boost::asio::async_connect(
                socket_ref, endpoints,
                [this, socket_ptr=std::move(socket_ptr), socket_promise = std::move(socket_promise)
                ](boost::system::error_code errc, boost::asio::ip::tcp::endpoint endpoint) mutable
                {
                    fmt::print(std::cout, "connector: handle connect errc {}, errmsg {}\n", errc, errc.message());
                    if (errc)
                    {
                        socket_promise.set_exception(std::runtime_error{ errc.message() });
                        return close_resolver(errc);
                    }
                    socket_promise.set_value(std::move(*socket_ptr.release()));
                });
        }

        void erase_pendlist_then_resolve(decltype(resolve_pendlist_)::DataType::iterator pendlist_iter)
        {
            auto const wlocked_pendlist = resolve_pendlist_.wlock();
            wlocked_pendlist->erase(pendlist_iter);
            if (wlocked_pendlist->empty())
            {
                auto const old_active_state = state_.reset(active, std::memory_order_release);
                return assert(old_active_state);
            }
            exec_resolve(wlocked_pendlist->begin());
        }

        void close_resolver(boost::system::error_code errc)
        {
            fmt::print(std::cerr, "resolver: close errc {}, errmsg {}\n", errc, errc.message());
            resolver_.cancel();
            using boost::hana::size_c;
            for (auto& tuple : *resolve_pendlist_.wlock())
                tuple[size_c<socket>].set_exception(std::runtime_error{ errc.message() });
            auto const old_active_state = state_.reset(active, std::memory_order_release);
            assert(old_active_state);
        }
    };

    template class connector<boost::asio::ip::tcp>;
}
