#pragma once

namespace net::server
{
    template<typename Protocal>
    class acceptor;

    template<>
    class acceptor<boost::asio::ip::tcp>
        : detail::state_base
        , protocal::tcp::protocal_base
    {
        using pending = boost::promise<boost::asio::ip::tcp::socket>;

        folly::Synchronized<std::list<pending>> socket_pendlist_;
        boost::asio::io_context& context_;
        boost::asio::ip::tcp::acceptor acceptor_;

    public:
        acceptor(boost::asio::ip::tcp::endpoint endpoint, boost::asio::io_context& context, bool reuse_addr = false)
            : context_(context)
            , acceptor_(context, endpoint, reuse_addr)
        {
            core::verify(acceptor_.is_open());
            fmt::print("acceptor: listen address {}, port {}\n", endpoint.address(), listen_port());
        }

        acceptor(uint16_t port, boost::asio::io_context& context, bool reuse_addr = false)
            : acceptor(boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4(), port }, context, reuse_addr)
        {}

        uint16_t listen_port() const
        {
            return acceptor_.local_endpoint().port();
        }

        template<typename Protocal, typename ...SessionParams>
        boost::future<session_ptr<Protocal>> listen_session(SessionParams&& ...args)
        {
            pending promise_socket;
            auto future_socket = promise_socket.get_future();
            {
                auto wlock = socket_pendlist_.wlock();
                wlock->emplace_back(std::move(promise_socket));
                if (wlock->size() <= 1)
                {
                    auto const inactive = is_active(true);
                    assert(!inactive);
                    boost::asio::post(context_, on_listen_session());
                }
            }
            using socket_type = boost::asio::ip::tcp::socket;
            return future_socket.then([this, args = std::forward_as_tuple(std::forward<SessionParams>(args)...)]
            (boost::future<boost::asio::ip::tcp::socket> future_socket) mutable
            {
                std::tuple<boost::asio::ip::tcp::socket, boost::asio::io_context&, SessionParams&&...>
                    args_tuple = std::tuple_cat(std::make_tuple(future_socket.get(), std::ref(context_)), std::move(args));
                return std::apply([this](boost::asio::ip::tcp::socket& socket,
                                         boost::asio::io_context& context,
                                         SessionParams&& ...args) -> session_ptr<Protocal>
                                  {
                                      return std::make_unique<session<Protocal>>(std::move(socket), context,
                                                                                 std::forward<SessionParams>(args)...);
                                  }, args_tuple);
            });
        }

    private:
        folly::Function<void() const>
            on_listen_session()
        {
            return [this]
            {
                acceptor_.async_accept(on_accept());
            };
        }

        folly::Function<void(boost::system::error_code errc, boost::asio::ip::tcp::socket socket)>
            on_accept()
        {
            return [this](boost::system::error_code errc, boost::asio::ip::tcp::socket socket)
            {
                fmt::print(std::cout, "acceptor: handle accept errc {}, errmsg {}\n", errc, errc.message());
                auto wlock = socket_pendlist_.wlock();
                if (errc)
                {
                    for (auto& socket_pending : *wlock)
                        socket_pending.set_exception(std::make_exception_ptr(std::runtime_error{ "acceptor error" }));
                    wlock->clear();
                    return close_acceptor(errc);
                }
                if (wlock->size() > 1)
                    boost::asio::post(context_, on_listen_session());
                else
                {
                    auto const active = is_active(false);
                    assert(active);
                }
                fmt::print(std::cout, "acceptor: on_accept, local {}, remote {}\n", socket.local_endpoint(), socket.remote_endpoint());
                wlock->front().set_value(std::move(socket));
                wlock->pop_front();
            };
        }

        void close_acceptor(boost::system::error_code errc)
        {
            fmt::print(std::cerr, "acceptor: close errc {}, errmsg {}\n", errc, errc.message());
            acceptor_.cancel();
            acceptor_.close();
        }
    };

    template class acceptor<boost::asio::ip::tcp>;
}
