// client.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

namespace
{
    std::ofstream ofs{ "C:/Media/Recv/recv.h264",std::ios::binary | std::ios::trunc | std::ios::out };
}

int main(int argc, char* argv[])
{
    try
    {   
        const auto io_context_ptr = std::make_shared<boost::asio::io_context>();
        auto work_guard = make_work_guard(*io_context_ptr);
        net::client client{ io_context_ptr };
        auto session_future = client.make_session("localhost", "8888");
        std::vector<std::thread> thread_pool(std::thread::hardware_concurrency());
        std::generate_n(thread_pool.begin(), thread_pool.size(),
            [io_context_ptr]{ return std::thread{ [io_context_ptr] { io_context_ptr->run(); } }; });
        const auto session_weak_ptr = session_future.get();
        fmt::print("start receiving\n");
        assert(!session_weak_ptr.expired());
        const std::string_view delim{ "(delim)" };
        const auto session_ptr = session_weak_ptr.lock();
        while(true)
        {
            auto recv_buffer = session_ptr->receive(delim);
            fmt::print("\t recv_buffer size: {}\n", recv_buffer.size());
            std::copy_n(recv_buffer.cbegin(), recv_buffer.size(), std::ostreambuf_iterator<char>{ofs});
            if (recv_buffer.size() < 128)
            {
                session_ptr->close_socket();
                break;
            }
        }
        work_guard.reset();
        for (auto& thread : thread_pool)
            if (thread.joinable()) thread.join();
    }
    catch (const std::exception& e)
    {
        fmt::print(std::cerr, "exception detected: {}\n", e.what());
    }
    return 0;
}