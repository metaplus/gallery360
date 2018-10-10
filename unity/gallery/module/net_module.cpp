#include "stdafx.h"
#include "export.h"

namespace
{
    int16_t asio_thread_count = 0;
}

namespace unity
{
    void _nativeConfigureNet(INT16 threads) {
        asio_thread_count = threads;
    }
}

namespace dll::net_module
{
    std::unique_ptr<boost::asio::io_context> io_context;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> io_context_guard;
    std::unique_ptr<net::client::connector<net::protocal::tcp>> connector;
    std::unique_ptr<folly::NamedThreadFactory> net_thread_factory;
    std::vector<std::thread> net_threads;

    boost::asio::io_context& initialize() {
        io_context = std::make_unique<boost::asio::io_context>();
        io_context_guard = std::make_unique<boost::asio::executor_work_guard<
            boost::asio::io_context::executor_type>>(io_context->get_executor());
        net_threads.clear();
        net_threads.resize(asio_thread_count);
        net_thread_factory = create_thread_factory("NetModule");
        std::generate(net_threads.begin(), net_threads.end(), [] {
            return net_thread_factory->newThread([] { io_context->run(); });
                      });
        connector = std::make_unique<decltype(connector)::element_type>(*io_context);
        return *io_context;
    }

    void release() {
        //connector->fail_promises_then_close_resolver(boost::system::error_code{});
        io_context_guard->reset();
        io_context->stop();
        for (auto& thread : net_threads) {
            if (thread.joinable())
                thread.join();
        }
        net_threads.clear();
    }

    boost::future<net_session_ptr> net_module::establish_http_session(folly::Uri const& uri) {
        auto port = std::to_string(uri.port());
        return connector->establish_session<protocal_type, response_body>(uri.host(), std::move(port));
    }

    std::tuple<std::string_view, std::string_view, std::string_view> split_url_components(std::string_view url) {
        std::string_view const protocal_sep{ "//" };
        std::string_view const port_sep{ ":" };
        if (auto const pos = url.find(protocal_sep); pos != std::string_view::npos)
            url.remove_prefix(pos + protocal_sep.size());
        auto pos = url.find('/');
        auto  host = url.substr(0, pos);
        pos = host.find(port_sep);
        auto  port = host.substr(pos + 1);
        host = host.substr(0, pos);
        auto  target = url.substr(pos);
        return std::make_tuple(host, port, target);
    }
}

