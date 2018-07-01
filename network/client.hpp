#pragma once

namespace net::client
{
    template<typename Protocal, typename Socket = boost::asio::ip::tcp::socket>
    class session;

    template<>
    class session<protocal::http, boost::asio::ip::tcp::socket>
    {
        enum state_index { active, chunked, state_size };

        folly::AtomicBitSet<state_size> state_;
        boost::asio::io_context& context_;
        boost::asio::ip::tcp::socket socket_;
        boost::beast::flat_buffer recvbuf_;
        boost::asio::io_context::strand mutable strand_;

        session(boost::asio::ip::tcp::socket&& socket,
                boost::asio::io_context& context)
            : context_(context)
            , socket_(std::move(socket))
            , strand_(context_)
        {
            assert(socket_.is_open());
            fmt::print(std::cout, "session: socket peer endpoint {}/{}\n", socket_.local_endpoint(), socket_.remote_endpoint());
        }
    };
}
