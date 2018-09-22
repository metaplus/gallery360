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
            std::variant<
                boost::promise<socket_type>,
                folly::Promise<socket_type>
            > socket;

            void set_exception(boost::system::error_code errc);
            void set_socket(socket_type&& socket);
        };

        struct detail
        {
            template<typename Protocal, typename ...Policy>
            using future_session_of = std::conditional_t<
                meta::is_within<core::folly_tag, Policy...>::value,
                folly::SemiFuture<session_ptr<Protocal>>,
                boost::future<session_ptr<Protocal>>
            >;
        };

        folly::Synchronized<std::list<pending>> resolve_pendlist_;
        boost::asio::io_context& context_;
        boost::asio::ip::tcp::resolver resolver_;

    public:
        explicit connector(boost::asio::io_context& context);

        void insert_pendlist(pending&& pending);

        void close_promises_and_resolver(boost::system::error_code errc);

        template<typename Protocal, typename ...Policy>
        detail::future_session_of<Protocal, Policy...>
            establish_session(std::string host, std::string service, Policy ...policy) {
            auto[promise_socket, future_socket] = core::promise_contract_of<socket_type>(policy...);
            insert_pendlist(pending{ host,service,std::move(promise_socket) });
            return create_session<Protocal>(std::move(future_socket));
        }

        template<typename Protocal>
        boost::future<session_ptr<Protocal>> create_session(boost::future<socket_type> future_socket) {
            return future_socket.then(
                boost::launch::deferred,
                [this](boost::future<socket_type> future_socket) {
                    return session<Protocal>::create(future_socket.get(), context_);
                });
        }

        template<typename Protocal>
        folly::SemiFuture<session_ptr<Protocal>> create_session(folly::SemiFuture<socket_type> future_socket) {
            return std::move(future_socket).deferValue(
                [this](socket_type socket) {
                    return session<Protocal>::create(std::move(socket), context_);
                });
        }

    private:
        folly::Function<void() const> on_establish_session(std::list<pending>::iterator pending);

        folly::Function<void(boost::system::error_code errc,
                             boost::asio::ip::tcp::resolver::results_type endpoints) const>
            on_resolve(std::list<pending>::iterator pending);
    };
}
