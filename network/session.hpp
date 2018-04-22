#pragma once

/*namespace core
{
    //  TODO: experimental
    template<typename Callable>
    struct dereference_callable
    {
        template<typename ...Types>
        decltype(auto) operator()(Types&& ...args) const
        //    ->  std::invoke_result_t<std::decay_t<Callable>, decltype(*std::forward<Types>(args))...>
        {
            return std::decay_t<Callable>{}((*std::forward<Types>(args))...);
        }
    };
}*/

namespace net
{
    template<typename Protocal>
    class session;

    template<>
    class session<core::as_element_t>
    {
    public:
        struct dereference_hash
        {
            template<typename SocketProtocal>
            size_t operator()(const boost::asio::basic_socket<SocketProtocal>& sock) const
            {
                return std::hash<std::string>{}(endpoint_string(sock));
            }
            template<typename SessionProtocal>
            size_t operator()(const std::shared_ptr<session<SessionProtocal>>& sess) const
            {
                return operator()(sess->socket());
            }
        };
        struct dereference_equal
        {
            template<typename SessionProtocal>
            bool operator()(const std::shared_ptr<session<SessionProtocal>>& lsess, 
                const std::shared_ptr<session<SessionProtocal>>& rsess) const
            {
                return *lsess == *rsess;
            }
        };
        class simplex_sequence
        {
        protected:
            simplex_sequence() = delete;
            struct invald_identity_error : std::logic_error
            {
                using std::logic_error::logic_error;
                using std::logic_error::operator=;
            };
            bool is_buffer_empty() const noexcept
            {
                return queue_.empty();
            }
        private:
            std::deque<boost::asio::const_buffer> queue_;
            mutable boost::asio::io_context::strand strand_;
            mutable boost::asio::steady_timer timer_;
            //const enum identity { reader, writer } identity_;
        };
    protected:
        template<typename SocketProtocal>
        static std::string endpoint_string(const boost::asio::basic_socket<SocketProtocal>& sock)
        {
            std::ostringstream oss;
            oss << sock.local_endpoint() << sock.remote_endpoint();
            return oss.str();
        }
    };

    using session_element = session<core::as_element_t>;

    template<>
    class session<boost::asio::ip::tcp>
        : public std::enable_shared_from_this<session<boost::asio::ip::tcp>>
    {
    public:
        explicit session(boost::asio::ip::tcp::socket socket)
            : socket_(std::move(socket))
            , socket_strand_(socket_.get_io_context())
            , read_strand_(socket_.get_io_context())
            , write_strand_(socket_.get_io_context())
            , hash_index_(session_element::dereference_hash{}(socket_))
        {
            core::verify(socket.is_open());
            fmt::print(std::cout, "socket connected, {}/{}\n", socket.local_endpoint(), socket.remote_endpoint());
        }
        virtual ~session() = default;
        bool operator<(const session& that) const
        {
            return is_index_valid() && that.is_index_valid() ?
                hash_index_ < that.hash_index_ :
                socket_.local_endpoint() < that.socket_.local_endpoint()
                || !(that.socket_.local_endpoint() < socket_.local_endpoint())
                && socket_.remote_endpoint() < that.socket_.remote_endpoint();
        }
        bool operator==(const session& that) const
        {
            return is_index_valid() && that.is_index_valid() ?
                hash_index_ == that.hash_index_ : !(*this < that) && !(that < *this);
        }
    protected:
        // virtual void do_write() = 0;
        // virtual void do_read() = 0;
        template<typename Callable>
        boost::asio::executor_binder<std::decay_t<Callable>, boost::asio::io_context>
            make_read_handler(Callable&& handler)
        {
            return boost::asio::bind_executor(read_strand_, std::forward<Callable>(handler));
        }
        template<typename Callable>
        boost::asio::executor_binder<std::decay_t<Callable>, boost::asio::io_context>
            make_write_handler(Callable&& handler)
        {
            return boost::asio::bind_executor(write_strand_, std::forward<Callable>(handler));
        }
        boost::asio::ip::tcp::socket socket_;
    private:
        const boost::asio::ip::tcp::socket& socket() const noexcept
        { 
            return socket_;
        }
        bool is_index_valid() const noexcept
        {
            return hash_index_ != std::numeric_limits<size_t>::infinity();
        }
        std::deque<boost::asio::const_buffer> read_sequence_;
        std::deque<boost::asio::const_buffer> write_sequence_;
        mutable boost::asio::io_context::strand socket_strand_;
        mutable boost::asio::io_context::strand read_strand_;
        mutable boost::asio::io_context::strand write_strand_;
        //mutable boost::asio::steady_timer read_timer_;
        //mutable boost::asio::steady_timer write_timer_;
        const size_t hash_index_ = std::numeric_limits<size_t>::infinity();
        friend session_element::dereference_hash;
        friend session_element::dereference_equal;
    };
    
    //  TODO:  template<> class session<boost::asio::ip::udp>;

    //template class session<boost::asio::ip::tcp>;
    //template class session<boost::asio::ip::udp>;

    using tcp_session = session<boost::asio::ip::tcp>;
    using udp_session = session<boost::asio::ip::udp>;
}
