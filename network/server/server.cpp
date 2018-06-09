// server.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

namespace http = boost::beast::http;

int main(int argc, char* argv[])
{   
    try
    {
        auto const io_context = std::make_shared<boost::asio::io_context>();
        std::vector<std::thread> thread_pool(std::thread::hardware_concurrency());
        auto const thread_guard = core::make_guard(
            [&thread_pool] { for (auto& thread : thread_pool) if (thread.joinable()) thread.join(); });
        auto const work_guard = boost::asio::make_work_guard(*io_context);
        std::generate_n(thread_pool.begin(), thread_pool.size(), [io_context]()
        {
            return std::thread{ [io_context]()
            {
                try { io_context->run(); }
                catch (std::exception const& e) { core::inspect_exception(e); }
            } };
        });
#ifdef SERVER_USE_LEGACY
        using protocal = boost::asio::ip::tcp;
        const auto server = std::make_shared<net::v1::server<protocal>>(io_context, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4(),6666 });
        av::format_context format{ av::source::path{ "C:/Media/H264/NewYork.h264" } };
        {
            auto session_future = server->accept_session();
            std::shared_ptr<net::v1::server<protocal>::session> session;
            const std::string_view delim{ "(delim)" };
            av::packet packet{ nullptr };
            while (!(packet = format.read(av::media::video{})).empty())
            {
                if (!packet->flags) continue;
                if (!session)
                {
                    session = session_future.get();
                    session->recv_delim_suffix(delim);
                }
                fmt::print("start sending, size: {}\n", packet.bufview().size());
                session->send(boost::asio::buffer(packet.bufview()), packet);
            }
            fmt::print("last sending\n");
            std::string empty_sendbuf;
            session->send(boost::asio::buffer(empty_sendbuf));
            session->close_socket(core::defer_execute);
        }
#endif // SERVER_USE_LEGACY
        boost::asio::ip::tcp::endpoint const endpoint{ boost::asio::ip::tcp::v4(),8900 };
        auto const acceptor = std::make_shared<net::server::acceptor<boost::asio::ip::tcp>>(endpoint, io_context);
        while (true)
        {
            fmt::print("server session waiting\n");
            auto const server_session = acceptor->listen_session<net::protocal::http>("C:/Media", net::use_chunk);
            server_session->run();
            fmt::print("server session complete\n");
        }
    }
    catch(std::exception const& e)
    {
        fmt::print(std::cerr, "main thread exception: {}\n", e.what());
        core::inspect_exception(e);
    }
    fmt::print("application quit\n");
    return 0;
}