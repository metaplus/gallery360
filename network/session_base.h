#pragma once

namespace net::detail
{
    template<typename Socket, typename RecvBuffer>
    class session_base;

    template<typename Protocal, typename RecvBuffer>
    class session_base<boost::asio::basic_stream_socket<Protocal>, RecvBuffer>
    {
        enum state_index { active, serialize, chunked, state_size };
        folly::AtomicBitSet<state_size> state_;

    protected:
        using buffer_type = RecvBuffer;

        boost::asio::io_context& context_;
        boost::asio::basic_stream_socket<Protocal> socket_;
        buffer_type recvbuf_;
        mutable std::atomic<int64_t> round_trip_index_ = -1;

        // static_assert(boost::asio::is_mutable_buffer_sequence<buffer_type>::value);

        session_base(boost::asio::basic_stream_socket<Protocal>&& socket,
                     boost::asio::io_context& context)
            : context_(context)
            , socket_(std::move(socket))
        {}

        void reserve_recvbuf_capacity(size_t size = boost::asio::detail::default_max_transfer_size)
        {
            recvbuf_.prepare(size);
        }

        void close_socket(boost::asio::socket_base::shutdown_type operation)
        {
            socket_.cancel();
            socket_.shutdown(operation);
        }

        void close_socket(boost::system::error_code errc, boost::asio::socket_base::shutdown_type operation)
        {
            fmt::print(std::cerr, "session: socket close, errc {}, errmsg {}\n", errc, errc.message());
            close_socket(operation);
        }

        template<typename U>
        void fail_promise_then_close_socket(boost::promise<U>& promise, boost::system::error_code errc,
                                            boost::asio::socket_base::shutdown_type operation = boost::asio::socket_base::shutdown_both)
        {
            promise.set_exception(std::runtime_error{ errc.message() });
            close_socket(errc, operation);
        }

        template<typename U>
        void fail_promise_then_close_socket(promise_base<U>& promise, boost::system::error_code errc,
                                            boost::asio::socket_base::shutdown_type operation = boost::asio::socket_base::shutdown_both)
        {
            promise.set_exception(errc.message());
            close_socket(errc, operation);
        }

        bool is_active() const
        {
            return state_.test(active, std::memory_order_acquire);
        }

        bool is_active(bool active)
        {
            return state_.set(state_index::active, active, std::memory_order_release);
        }

        bool is_serialize() const
        {
            return state_.test(serialize, std::memory_order_acquire);
        }

        bool is_serialize(bool active)
        {
            return state_.set(state_index::serialize, serialize, std::memory_order_release);
        }

        bool is_chunked() const
        {
            return state_.test(chunked, std::memory_order_acquire);
        }

        bool is_chunked(bool active)
        {
            is_serialize(active);
            return state_.set(state_index::chunked, chunked, std::memory_order_release);
        }

        boost::asio::ip::tcp::endpoint local_endpoint() const
        {
            return socket_.local_endpoint();
        }

        boost::asio::ip::tcp::endpoint remote_endpoint() const
        {
            return socket_.local_endpoint();
        }
    };
}
