#pragma once

namespace detail
{
    template<typename Socket, typename Buffer>
    class session_base;

    template<typename Protocal, typename Buffer>
    class session_base<boost::asio::basic_stream_socket<Protocal>, Buffer>
    {
    protected:
        using buffer_type = Buffer;

        boost::asio::io_context& context_;
        boost::asio::basic_stream_socket<Protocal> socket_;
        buffer_type recvbuf_;
        mutable int64_t round_index_ = -1;

        // static_assert(boost::asio::is_mutable_buffer_sequence<buffer_type>::value);

        session_base(boost::asio::basic_stream_socket<Protocal>&& socket,
                     boost::asio::io_context& context)
            : context_(context)
            , socket_(std::move(socket)) {}

        bool operator<(const session_base& that) const {
            using basic_socket_type = boost::asio::basic_socket<Protocal>;
            return std::less<basic_socket_type>{}(socket_, that.socket_);
        }

        void reserve_recvbuf_capacity(size_t size = boost::asio::detail::default_max_transfer_size) {
            recvbuf_.prepare(size);
        }

        template<
            template<typename> typename Container,
            typename Message,
            typename Exception
        >
        void clear_promise_list(folly::Synchronized<Container<folly::Promise<Message>>>& promise_list,
                                const Exception& exception) {
            promise_list.withWLock(
                [this, &exception](Container<folly::Promise<Message>>& promise_list) {
                    for (folly::Promise<Message>& promise : promise_list) {
                        promise.setException(exception);
                    }
                });
        }

        void close_socket(boost::asio::socket_base::shutdown_type operation) {
            socket_.cancel();
            socket_.shutdown(operation);
        }

        void close_socket(boost::system::error_code errc,
                          boost::asio::socket_base::shutdown_type operation) {
            close_socket(operation);
        }

        template<typename U>
        void close_promise_and_socket(boost::promise<U>& promise,
                                      boost::system::error_code errc,
                                      boost::asio::socket_base::shutdown_type operation = boost::asio::socket_base::shutdown_both) {
            promise.set_exception(std::runtime_error{ errc.message() });
            close_socket(errc, operation);
        }

        template<typename U>
        void close_promise_and_socket(folly::Promise<U>& promise,
                                      boost::system::error_code errc,
                                      boost::asio::socket_base::shutdown_type operation = boost::asio::socket_base::shutdown_both) {
            promise.setException(std::runtime_error{ errc.message() });
            close_socket(errc, operation);
        }

        template<typename U>
        void close_promise_and_socket(std::variant<boost::promise<U>, folly::Promise<U>>& promise,
                                      boost::system::error_code errc,
                                      boost::asio::socket_base::shutdown_type operation = boost::asio::socket_base::shutdown_both) {
            core::visit(promise,
                        [errc](folly::Promise<U>& promise) {
                            promise.setException(std::runtime_error{ errc.message() });
                        },
                        [errc](boost::promise<U>& promise) {
                            promise.set_exception(std::runtime_error{ errc.message() });
                        });
            close_socket(errc, operation);
        }

        #ifdef NET_USE_PROMISE_BASE
        template<typename U>
        void fail_promise_then_close_socket(promise_base<U>& promise, boost::system::error_code errc,
                                            boost::asio::socket_base::shutdown_type operation = boost::asio::socket_base::shutdown_both) {
            promise.set_exception(errc.message());
            close_socket(errc, operation);
        }
        #endif

        boost::asio::ip::tcp::endpoint local_endpoint() const {
            return socket_.local_endpoint();
        }

        boost::asio::ip::tcp::endpoint remote_endpoint() const {
            return socket_.remote_endpoint();
        }
    };
}
