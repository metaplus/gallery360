#pragma once

namespace net::client
{
    template<typename Protocal>
    class connector;

    template<>
    class connector<protocal::tcp>
        : state_base
        , protocal::tcp::protocal_base
    {
        struct pending
        {
            std::string host;
            std::string service;
            boost::promise<socket_type> promise;
        };

        folly::Synchronized<std::list<pending>> resolve_pendlist_;
        boost::asio::io_context& context_;
        boost::asio::ip::tcp::resolver resolver_;

    public:
        explicit connector(boost::asio::io_context& context);

        boost::future<boost::asio::ip::tcp::socket> async_connect_socket(pending&& pending);
        void fail_promises_then_close_resolver(boost::system::error_code errc);

        template<typename Protocal, typename ResponseBody>
        boost::future<session_ptr<Protocal, policy<ResponseBody>>>
            establish_session(std::string host, std::string service) {
            pending pending{ std::move(host),std::move(service),boost::promise<socket_type>{} };
            return async_connect_socket(std::move(pending)).then(
                boost::launch::deferred,
                [this](boost::future<boost::asio::ip::tcp::socket> future_socket) {
                    return std::make_unique<session<Protocal, policy<ResponseBody>>>(
                        future_socket.get(), resolver_.get_executor().context());
                });
        }

    private:
        folly::Function<void() const>
            on_establish_session(std::list<pending>::iterator pending);
        folly::Function<void(boost::system::error_code errc, boost::asio::ip::tcp::resolver::results_type endpoints) const>
            on_resolve(std::list<pending>::iterator pending);
        folly::Function<void(boost::system::error_code errc, boost::asio::ip::tcp::endpoint endpoint)>
            on_connect(std::unique_ptr<boost::asio::ip::tcp::socket> socket_ptr,
                       boost::promise<socket_type>&& promise_socket);
    };

    template class connector<protocal::tcp>;
}
