#pragma once

namespace net::client
{

    template<typename Protocal, typename Socket = boost::asio::ip::tcp::socket>
    class session;

    template<>
    class session<protocal::http, boost::asio::ip::tcp::socket>
        : public std::enable_shared_from_this<session<protocal::http, boost::asio::ip::tcp::socket>>
    {
        std::shared_ptr<boost::asio::io_context> const execution_;
        boost::asio::ip::tcp::socket socket_;
        boost::beast::flat_buffer recvbuf_;
        boost::asio::io_context::strand mutable strand_;
        std::atomic<bool> mutable active_{ false };

        session(boost::asio::ip::tcp::socket sock, std::shared_ptr<boost::asio::io_context> ctx)
            : execution_(std::move(ctx))
            , socket_(std::move(sock))
            , strand_(*execution_)
        {
            assert(socket_.is_open());
            assert(core::address_same(*execution_, socket_.get_executor().context()));
            fmt::print(std::cout, "socket peer endpoint {}/{}\n", socket_.local_endpoint(), socket_.remote_endpoint());
        }

    };


}