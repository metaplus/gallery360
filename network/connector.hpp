#pragma once

namespace net::client
{
    template<typename Protocal>
    class connector;

    template<>
    class connector<boost::asio::ip::tcp>
        : public std::enable_shared_from_this<connector<boost::asio::ip::tcp>>
    {
        boost::asio::ip::tcp::resolver resolver_;
        std::list<
            std::tuple<std::string_view, std::string_view, boost::promise<boost::asio::ip::tcp::socket>>
        > resolve_pendinglist_;
        std::unordered_map<
            std::pair<std::string_view, std::string_view>,
            boost::asio::ip::tcp::resolver::results_type,
            boost::hash<std::pair<std::string_view, std::string_view>>
        > endpoint_cache_;
        // std::promise<std::shared_ptr<boost::asio::ip::tcp::socket>> socket_promise_;
        boost::asio::io_context::strand mutable resolver_strand_;
        boost::asio::io_context::strand mutable resolve_pendinglist_strand_;
        std::atomic<bool> mutable active_{ false };

    public:
        explicit connector(boost::asio::io_context& context)
            : resolver_(context)
            , resolver_strand_(context)
            , resolve_pendinglist_strand_(context)
        {}

        template<typename ApplicationProtocal, typename ...SessionArgs>
        std::shared_ptr<session<ApplicationProtocal, boost::asio::ip::tcp::socket>>
            establish_session(std::string_view host, std::string_view service, SessionArgs&& ...args)
        {
            boost::promise<boost::asio::ip::tcp::socket> socket_promise;
            auto socket_future = socket_promise.get_future();
            boost::asio::post(resolve_pendinglist_strand_,
                              [socket_promise = std::move(socket_promise), this, self = shared_from_this()]() mutable
            {
                resolve_pendinglist_.push_back(std::move(socket_promise));
                // if (resolve_pendinglist_.size() > 1) return;
                boost::asio::post(resolver_strand_,
                                  [=, self, socket_iter = resolve_pendinglist_.begin()]{ exec_resolve(host,service,socket_iter); });
            });
            return std::make_shared<session<ApplicationProtocal, boost::asio::ip::tcp::socket>>(
                socket_future.get(), resolver_.get_executor().context(), std::forward<SessionArgs>(args)...);
        }

    private:
        void exec_resolve(decltype(resolve_pendinglist_)::reference resolve_pending)
        {
            auto&[host, service, socket_promise] = resolve_pending;
            assert(resolver_strand_.running_in_this_thread());
            auto const endpoints_iter = endpoint_cache_.find(std::make_pair(host, service));
            if (endpoints_iter != endpoint_cache_.end())
            {
                boost::asio::dispatch(resolve_pendinglist_strand_, [=] { extract_pendinglist(endpoints_iter); });
                return;
            }
            resolver_.async_resolve(
                host, service, bind_executor(resolver_strand_,
                                             [=, self = shared_from_this()](boost::system::error_code errc, boost::asio::ip::tcp::resolver::results_type endpoints)
            {
                //if (resolve_pendinglist_.size())

                if (errc)
                {
                    socket_iter->set_exception(std::make_exception_ptr(std::runtime_error{ errc.message() }));
                    return fmt::print(std::cerr, "resolver: resolve errc {}, errmsg {}\n", errc, errc.message());
                }

                auto const[endpoints_iter, success] = endpoint_cache_.emplace(std::make_pair(host, service), std::move(endpoints));
                assert(success);
                extract_pendinglist(endpoints_iter);

            }));

        }

        void exec_connect(decltype(endpoint_cache_)::mapped_type const& endpoints, decltype(resolve_pendinglist_)::reference socket_promise)
        {
            assert(resolver_.get_executor().running_in_this_thread());
            auto const socket = std::make_shared<boost::asio::ip::tcp::socket>(resolver_.get_executor().context());
            boost::asio::async_connect(
                *socket, endpoints, // bind_executor(resolver_strand_,
                [=, socket_promise = std::move(socket_promise), self = shared_from_this()
                ](boost::system::error_code errc, boost::asio::ip::tcp::endpoint endpoint) mutable
            {
                if (errc)
                {
                    socket_promise.set_exception(std::make_exception_ptr(std::runtime_error{ errc.message() }));
                    fmt::print(std::cerr, "accept errc {}, errmsg {}\n", errc, errc.message());
                } else socket_promise.set_value(std::move(*socket));
                boost::asio::dispatch(resolve_pendinglist_strand_, [=] { resolve_pendinglist_.erase(socket_iter); });
            });
        }

        void extract_pendinglist(decltype(endpoint_cache_)::iterator endpoints_iter)
        {
            assert(resolve_pendinglist_strand_.running_in_this_thread());
            auto socket_promise = std::move(resolve_pendinglist_.front());
            resolve_pendinglist_.pop_front();
            if (resolve_pendinglist_.size())
                boost::asio::post(resolver_strand_, [])
                auto const& endpoints_ref = endpoints_iter->second;
            boost::asio::post(resolver_.get_executor(),
                              [socket_promise = std::move(socket_promise), &endpoints_ref, this, self = shared_from_this()
                              ]() mutable { exec_connect(endpoints_ref, socket_promise); });

        }

        void close_resolver()
        {
            auto const active_old = std::atomic_exchange(&active_, false);
            assert(active_old);
            resolver_.cancel();
        }
    };

    template class connector<boost::asio::ip::tcp>;
}