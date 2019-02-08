#pragma once
#include "network/session.server.h"

namespace net::server
{
    template <typename Protocal>
    class acceptor;

    template <>
    class acceptor<boost::asio::ip::tcp> final :
        protocal::protocal_base<protocal::tcp>
    {
        boost::asio::io_context& context_;
        boost::asio::ip::tcp::acceptor acceptor_;

    public:
        acceptor(boost::asio::ip::tcp::endpoint endpoint,
                 boost::asio::io_context& context, bool reuse_addr = false);

        acceptor(uint16_t port,
                 boost::asio::io_context& context, bool reuse_addr = false);

        uint16_t listen_port() const;

        folly::SemiFuture<socket_type> accept_socket();

        template <typename Protocal, typename ...SessionArgs>
        folly::SemiFuture<session_ptr<Protocal>> listen_session(SessionArgs&& ...args) {
            return accept_socket().deferValue(
                [this, &args...](socket_type&& socket) {
                    return session<Protocal>::create(std::move(socket), context_,
                                                     std::forward<SessionArgs>(args)...);
                });
        }

        void close(std::optional<boost::system::error_code> error = std::nullopt,
                   bool cancel = false);

    private:
        auto on_accept(folly::Promise<socket_type>&& promise);
    };

    template class acceptor<boost::asio::ip::tcp>;
}
