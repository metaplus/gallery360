#include "stdafx.h"
#include "export.h"

namespace dll::net_module
{
    std::unique_ptr<boost::asio::io_context> io_context;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> io_context_guard;
    std::unique_ptr<net::connector<net::protocal::tcp>> connector;
    std::unique_ptr<folly::NamedThreadFactory> net_thread_factory;
    std::vector<std::thread> net_threads;

    boost::asio::io_context& initialize()
    {
        auto const thread_capacity = std::thread::hardware_concurrency() / 4;
        io_context = std::make_unique<boost::asio::io_context>();
        io_context_guard = std::make_unique<boost::asio::executor_work_guard<
            boost::asio::io_context::executor_type>>(io_context->get_executor());
        net_threads.clear();
        net_threads.resize(thread_capacity);
        net_thread_factory = create_thread_factory("NetModule");
        std::generate(net_threads.begin(), net_threads.end(), []
                      {
                          return net_thread_factory->newThread([] { io_context->run(); });
                      });
        connector = std::make_unique<decltype(connector)::element_type>(*io_context);
        return *io_context;
    }

    void release()
    {
        //connector->fail_promises_then_close_resolver(boost::system::error_code{});
        io_context_guard->reset();
        io_context->stop();
        for (auto& thread : net_threads)
        {
            if (thread.joinable())
                thread.join();
        }
        net_threads.clear();
    }

    std::unique_ptr<client_session> net_module::establish_http_session(std::string_view host, std::string_view service)
    {
        return connector->establish_session<protocal_type, response_body>(host, service);
    }

    std::pair<std::string_view, std::string_view> split_url_components(std::string_view url)
    {
        if (auto const pos = url.find("//"); pos != std::string_view::npos)
            url.remove_prefix(pos + 1);
        auto const pos = url.find('/');
        auto const host = url.substr(0, pos);
        auto const target = url.substr(pos);
        return std::make_pair(host, target);
    }
}

