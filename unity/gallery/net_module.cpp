#include "stdafx.h"
#include "export.h"

namespace dll::net_module
{
    std::unique_ptr<boost::asio::io_context> io_context;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> io_context_guard;
    std::unique_ptr<net::connector<net::protocal::tcp>> connector;

    boost::asio::io_context& initialize(boost::thread_group& thread_group)
    {
        auto const thread_capacity = std::thread::hardware_concurrency() / 4;
        io_context = std::make_unique<boost::asio::io_context>();
        io_context_guard = std::make_unique<boost::asio::executor_work_guard<
            boost::asio::io_context::executor_type>>(io_context->get_executor());
        std::vector<boost::thread*> threads(thread_capacity);
        std::generate(threads.begin(), threads.end(),
                      [&] { return thread_group.create_thread([] { io_context->run(); }); });
        connector = std::make_unique<decltype(connector)::element_type>(*io_context);
        return *io_context;
    }

    void release()
    {
        connector->fail_promises_then_close_resolver(boost::system::error_code{});
        io_context_guard->reset();
        io_context->stop();
    }

    std::unique_ptr<client_session> net_module::establish_http_session(std::string_view host, std::string_view service)
    {
        return connector->establish_session<protocal_type, response_body>(host, service);
    }
}

