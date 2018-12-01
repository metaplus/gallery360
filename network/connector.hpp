#pragma once

namespace net::client
{
    template<typename Protocal>
    class connector;

    template<>
    class connector<protocal::tcp> : protocal::base<protocal::tcp>
    {
        struct entry
        {
            std::string host;
            std::string service;
            folly::Promise<socket_type> socket;
        };

        folly::Synchronized<std::list<entry>> resolve_list_;
        boost::asio::io_context& context_;
        boost::asio::ip::tcp::resolver resolver_;

    public:
        explicit connector(boost::asio::io_context& context);

        void shutdown_and_reject_request(boost::system::error_code errc);

        template<typename Protocal>
        folly::SemiFuture<session_ptr<Protocal>>
        establish_session(std::string host,
                          std::string service) {
            auto [promise_socket, future_socket] = folly::makePromiseContract<socket_type>();
            resolve_list_.withWLock(
                [this, host, service, &promise_socket](std::list<entry>& resolve_list) {
                    const auto entry_iter = resolve_list.insert(resolve_list.end(),
                                                                entry{ host, service, std::move(promise_socket) });
                    if (resolve_list.size() <= 1) {
                        boost::asio::post(context_, [this, entry_iter] {
                            resolver_.async_resolve(entry_iter->host,
                                                    entry_iter->service,
                                                    on_resolve(entry_iter));
                        });
                    }
                });
            return std::move(future_socket).deferValue(
                [this](socket_type socket) {
                    return session<Protocal>::create(std::move(socket), context_);
                });
        }

    private:
        folly::Function<void(boost::system::error_code errc,
                             boost::asio::ip::tcp::resolver::results_type endpoints) const>
        on_resolve(std::list<entry>::iterator entry_iter);
    };
}
