// client.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

namespace
{
	std::ofstream ofs{ "C:/Media/Recv/recv.h264",std::ios::binary | std::ios::trunc | std::ios::out };
	std::ofstream ofs_yuv{ "C:/Media/Recv/recv.yuv",std::ios::binary | std::ios::trunc | std::ios::out };
}



int main(int argc, char* argv[])
{
	try
	{
        
        auto const port = net::config().get<uint16_t>("net.client.port");
        auto const directory = net::config().get<std::string>("net.client.directories.root");
        boost::asio::io_context io_context;
        auto const io_context_guard = boost::asio::make_work_guard(io_context);
        boost::thread_group thread_group;
        auto const thread_group_guard = core::make_guard([&] { thread_group.join_all(); });
        std::vector<boost::thread*> threads(boost::thread::hardware_concurrency());
        std::generate_n(threads.begin(), threads.size(),
                        [&] { return thread_group.create_thread([&] { io_context.run(); }); });
	}
	catch (std::exception const& exp) {
        core::inspect_exception(exp);
    } catch (boost::exception const& exp) {
        core::inspect_exception(exp);
    }
    fmt::print("app: application quit\n");
	return 0;
}