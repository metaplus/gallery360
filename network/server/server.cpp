// server.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

int main(int argc, char* argv[])
{
    try
    {
        const auto io_context_ptr = std::make_shared<boost::asio::io_context>();
        net::server server{ io_context_ptr,boost::asio::ip::tcp::endpoint{boost::asio::ip::tcp::v4(),8888} };
        av::format_context format{ av::source::path{ "C:/Media/H264/NewYork.h264" } };
        std::vector<std::thread> thread_pool(std::thread::hardware_concurrency());
        {
            const auto work_guard = make_work_guard(*io_context_ptr);
            std::generate_n(thread_pool.begin(), thread_pool.size(),
                [io_context_ptr] { return std::thread{ [io_context_ptr] { io_context_ptr->run(); } }; });
            auto session_ptr_future = server.wait_session();
            std::shared_ptr<net::server::session> session_ptr;
            const std::string_view delim{ "(delim)" };
            av::packet packet{ nullptr };
            while (!(packet = format.read(av::media::video{})).empty())
            {
                if (!packet->flags) continue;
                if (!session_ptr) session_ptr = session_ptr_future.get();
                fmt::print("start sending, size: {}\n", packet.cbuffer_view().size());
                session_ptr->send(boost::asio::buffer(packet.cbuffer_view()), packet);
                session_ptr->send(boost::asio::buffer(delim));
            }
            fmt::print("last sending\n");
            session_ptr->send(boost::asio::buffer(delim));
            session_ptr->close_socket_after_finish();
        }
        for (auto& thread : thread_pool)
            if (thread.joinable()) thread.join();
    }
    catch (const std::exception& e)
    {
        fmt::print(std::cerr, "exception detected: {}\n", e.what());
    }
    return 0;
}