#pragma once

namespace net::server
{
    template <typename Protocal>
    class acceptor;

    template <>
    class acceptor<boost::asio::ip::tcp> final : protocal::base<protocal::tcp>
    {
        using entry = folly::Promise<boost::asio::ip::tcp::socket>;

        folly::Synchronized<std::list<entry>> accept_list_;
        boost::asio::io_context& context_;
        boost::asio::ip::tcp::acceptor acceptor_;

    public:
        acceptor(boost::asio::ip::tcp::endpoint endpoint,
                 boost::asio::io_context& context,
                 bool reuse_addr = false);

        acceptor(uint16_t port,
                 boost::asio::io_context& context,
                 bool reuse_addr = false);

        uint16_t listen_port() const;

        folly::SemiFuture<socket_type> accept_socket();

        template <typename Protocal, typename ...SessionArgs>
        folly::SemiFuture<session_ptr<Protocal>> listen_session(SessionArgs&& ...args) {
            return accept_socket().deferValue(
                [this, &args...](socket_type&& socket) {
                    return session<Protocal>::create(std::move(socket),
                                                     context_,
                                                     std::forward<SessionArgs>(args)...);
                });
        }

        void close(bool cancel = false);

    private:
        folly::Function<void(boost::system::error_code errc,
                             boost::asio::ip::tcp::socket socket)>
        on_accept();

        void close_acceptor(boost::system::error_code errc);
    };

    template class acceptor<boost::asio::ip::tcp>;
}
