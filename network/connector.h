#pragma once
#include "network/net.h"
#include "network/session.client.h"
#include <boost/asio/strand.hpp>

namespace net::client
{
    template <typename Protocal>
    class connector;

    template <>
    class connector<protocal::tcp> final :
        protocal::protocal_base<protocal::tcp>
    {
        struct connect_token final
        {
            std::string host;
            std::string service;
            folly::Promise<socket_type> socket;
        };

        using connect_list = std::list<connect_token>;
        using connect_sequence = boost::asio::strand<boost::asio::io_context::executor_type>;

        boost::asio::io_context& context_;
        boost::asio::ip::tcp::resolver resolver_;
        connect_list connect_list_;
        connect_sequence connect_sequence_;

    public:
        explicit connector(boost::asio::io_context& context);

        void fail_socket_then_cancel(boost::system::error_code errc);

        folly::SemiFuture<socket_type> connect_socket(std::string_view host, std::string_view service);

        template <typename Protocal>
        folly::SemiFuture<session_ptr<Protocal>>
        establish_session(std::string_view host, std::string_view service) {
            return connect_socket(std::move(host), std::move(service))
                .deferValue(
                    [this](socket_type socket) {
                        return session<Protocal>::create(std::move(socket), context_);
                    });
        }

    private:
        auto on_resolve();

        void resolve_front_endpoint();
    };
}
