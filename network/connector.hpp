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
            void set_socket(socket_type& socket);
        };

        struct detail
        {
            template<typename ...Policy>
            using future_of = std::conditional_t<
                meta::is_within<folly::Promise<socket_type>, Policy...>::value,
                folly::SemiFuture<socket_type>,
                boost::future<socket_type>
            >;

            template<typename Protocal, typename ...Policy>
            using future_session_of = std::conditional_t<
                meta::is_within<folly::Promise<socket_type>, Policy...>::value,
                folly::SemiFuture<session_ptr<Protocal>>,
                boost::future<session_ptr<Protocal>>
            >;

            template<typename ...Policy>
            static auto promise_of(Policy& ...p) {
                if constexpr (meta::is_within<folly::Promise<socket_type>, Policy...>::value) {
                    auto tuple = std::forward_as_tuple(p...);
                    return std::move(std::get<folly::Promise<socket_type>&>(tuple));
                } else if constexpr (meta::is_within<boost::promise<socket_type>, Policy...>::value) {
                    auto tuple = std::forward_as_tuple(p...);
                    return std::move(std::get<boost::promise<socket_type>&>(tuple));
                } else {
                    return boost::promise<socket_type>{};
                }
            }

            template<typename ...Policy>
            static auto promise_contract_of(Policy& ...p) {
                if constexpr (meta::is_within<folly::Promise<socket_type>, Policy...>::value) {
                    auto tuple = std::forward_as_tuple(p...);
                    folly::Promise<socket_type>& promise = std::get<folly::Promise<socket_type>&>(tuple);
                    auto future = promise.getSemiFuture();
                    return std::make_pair(std::move(promise), std::move(future));
                } else if constexpr (meta::is_within<boost::promise<socket_type>, Policy...>::value) {
                    auto tuple = std::forward_as_tuple(p...);
                    boost::promise<socket_type>& promise = std::get<boost::promise<socket_type>&>(tuple);
                    auto future = promise.get_future();
                    return std::make_pair(std::move(promise), std::move(future));
                } else {
                    boost::promise<socket_type> promise;
                    auto future = promise.get_future();
                    return std::make_pair(std::move(promise), std::move(future));
                }
            }

            static_assert(std::is_same<boost::promise<socket_type>, decltype(promise_of())>::value);
            static_assert(std::is_same<boost::promise<socket_type>, decltype(promise_of(std::declval<boost::promise<socket_type>&>()))>::value);
            static_assert(std::is_same<folly::Promise<socket_type>, decltype(promise_of(std::declval<folly::Promise<socket_type>&>()))>::value);
        };

        folly::Synchronized<std::list<pending>> resolve_pendlist_;
        boost::asio::io_context& context_;
        boost::asio::ip::tcp::resolver resolver_;

    public:
        explicit connector(boost::asio::io_context& context);

        void insert_pendlist(pending&& pending);

        void close_promises_and_resolver(boost::system::error_code errc);

        template<typename Protocal, typename ...Policy>
        // boost::future<session_ptr<Protocal>>
        detail::future_session_of<Protocal, Policy...>
            establish_session(std::string host, std::string service, Policy ...policy) {
            auto[promise_socket, future_socket] = detail::promise_contract_of(policy...);
            insert_pendlist(pending{ std::move(host),std::move(service), std::move(promise_socket) });
            return create_session<Protocal>(std::move(future_socket));
        }

        template<typename Protocal>
        boost::future<session_ptr<Protocal>> create_session(
            boost::future<boost::asio::ip::tcp::socket> future_socket) {
            return future_socket.then(
                boost::launch::deferred,
                [this](boost::future<boost::asio::ip::tcp::socket> future_socket) {
                    return std::make_unique<session<Protocal>>(future_socket.get(), context_);
                });
        }

        template<typename Protocal>
        folly::SemiFuture<session_ptr<Protocal>> create_session(
            folly::SemiFuture<boost::asio::ip::tcp::socket> future_socket) {
            return future_socket.deferValue(
                [this](boost::asio::ip::tcp::socket socket) {
                    return std::make_unique<session<Protocal>>(std::move(socket), context_);
                });
        }

    private:
        folly::Function<void() const>
            on_establish_session(std::list<pending>::iterator pending);

        folly::Function<void(boost::system::error_code errc,
                             boost::asio::ip::tcp::resolver::results_type endpoints) const>
            on_resolve(std::list<pending>::iterator pending);
    };
}
