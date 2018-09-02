#pragma once

namespace net::server
{
    template<typename Protocal>
    class acceptor;

    template<>
    class acceptor<boost::asio::ip::tcp>
        : state_base
        , protocal::tcp::protocal_base
    {
        using pending = boost::promise<boost::asio::ip::tcp::socket>;

        folly::Synchronized<std::list<pending>> socket_pendlist_;
        boost::asio::io_context& context_;
        boost::asio::ip::tcp::acceptor acceptor_;

    public:
        acceptor(boost::asio::ip::tcp::endpoint endpoint, boost::asio::io_context& context, bool reuse_addr = false);
        acceptor(uint16_t port, boost::asio::io_context& context, bool reuse_addr = false);

        uint16_t listen_port() const;
        boost::future<socket_type> async_listen_socket(pending&& pending);

        template<typename Protocal, typename ...SessionParams>
        boost::future<session_ptr<Protocal>> listen_session(SessionParams&& ...params) {
            auto args = std::forward_as_tuple(std::forward<SessionParams>(params)...);
            return async_listen_socket(pending{}).then(
                [this, args](boost::future<boost::asio::ip::tcp::socket> future_socket) mutable {
                    std::tuple<boost::asio::ip::tcp::socket, boost::asio::io_context&, SessionParams&&...>
                        args_tuple = std::tuple_cat(std::make_tuple(future_socket.get(), std::ref(context_)), std::move(args));
                    return std::apply(
                        [this](boost::asio::ip::tcp::socket& socket, boost::asio::io_context& context,
                               SessionParams&& ...args) -> session_ptr<Protocal> {
                                   return std::make_unique<session<Protocal>>(std::move(socket), context,
                                                                              std::forward<SessionParams>(args)...);
                        }, args_tuple);
                });
        }

    private:
        folly::Function<void() const>
            on_listen_session();
        folly::Function<void(boost::system::error_code errc, boost::asio::ip::tcp::socket socket)>
            on_accept();
        void close_acceptor(boost::system::error_code errc);
    };

    template class acceptor<boost::asio::ip::tcp>;
}
