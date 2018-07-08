// client.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <boost/asio/buffer.hpp>
namespace
{
    auto const sample_url = "http://techslides.com/demos/sample-videos/small.mp4";
}

int main(int argc, char* argv[])
{
    try
    {
        auto const port = net::config().get<uint16_t>("net.client.port");
        auto const directory = net::config().get<std::string>("net.client.directories.root");
        std::vector<boost::thread*> threads(boost::thread::hardware_concurrency() / 2);
        std::multimap<
            boost::asio::ip::tcp::endpoint,
            std::unique_ptr<net::client::session<net::protocal::http>>
        > sessions;
        boost::thread_group thread_group;
        boost::asio::io_context io_context;
        boost::asio::ip::tcp::endpoint const endpoint{ boost::asio::ip::tcp::v4(), port };
        net::connector<boost::asio::ip::tcp> connector{ io_context };
        net::executor_guard executor_guard{ thread_group, io_context };
        std::generate_n(threads.begin(), threads.size(),
                        [&] { return thread_group.create_thread([&] { io_context.run(); }); });
        while (true)
        {
            fmt::print("app: client session waiting\n");
            std::string_view const host{ "www.techslides.com" };
            std::string_view const target{ "/demos/sample-videos/small.mp4" };
            if (auto session_ptr = connector.establish_session<net::protocal::http>(host, "80"); session_ptr != nullptr)
            {
                auto& session = *sessions.emplace(endpoint, std::move(session_ptr))->second;
                auto response = std::make_shared<boost::beast::http::response<
                    boost::beast::http::dynamic_body>>(session.async_send_request(host, target).get());
                std::vector<size_t> read_log;
                av::io_context io_source{
                    [recvbuf_iter = boost::asio::buffer_sequence_begin(response->body().data()),
                     recvbuf_end = boost::asio::buffer_sequence_end(response->body().data()),
                     read_offset = size_t{ 0 }, response, &read_log
                    ](uint8_t* buffer, int size) mutable->int
                    {
                        if (recvbuf_iter != recvbuf_end)
                        {
                            auto const recvbuf_size = (*recvbuf_iter).size();
                            auto const recvbuf_ptr = static_cast<char const*>((*recvbuf_iter).data());
                            auto const read_size = std::min<size_t>(size, recvbuf_size - read_offset);
                            std::copy_n(recvbuf_ptr + read_offset, read_size, buffer);
                            read_offset += read_size;
                            if (read_offset == recvbuf_size)
                            {
                                recvbuf_iter.operator++();
                                read_offset = 0;
                            }
                            read_log.push_back(read_size);
                            assert(read_size != 0);
                            return boost::numeric_cast<int>(read_size);
                        }
                        return AVERROR_EOF;
                    },nullptr,nullptr };
                av::format_context format{ io_source,av::source::format{"mp4"} };
                auto totalSize = std::accumulate(read_log.begin(), read_log.end(), 0ull);
                av::codec_context codec{ format,av::media::video{} };
                av::packet packet{ nullptr };
                size_t readCount = 0;
                size_t decodeCount = 0;
                while ((packet = format.read(av::media::video{})))
                {
                    ++readCount;
                    auto frames = codec.decode(packet);
                    decodeCount += frames.size();
                }
                break;
            }
            fmt::print("app: client session monitored\n\n");
        }
    }
    catch (std::exception const& exp)
    {
        core::inspect_exception(exp);
    }
    catch (boost::exception const& exp)
    {
        core::inspect_exception(exp);
    }
    fmt::print("app: application quit\n");
    return 0;
}
