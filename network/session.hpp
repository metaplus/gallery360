#pragma once

namespace net
{
    template<
        typename Protocal = boost::asio::ip::tcp,
        template<typename> typename Socket = boost::asio::basic_stream_socket
    > class session;

    class session_element
    {
    public:
        struct dereference_hash
        {
            template<typename Socket>
            size_t operator()(const Socket& sock) const
            {
                return std::hash<std::string>{}(endpoint_string<Socket>(sock));
            }

            template<typename Session>
            size_t operator()(const std::shared_ptr<Session>& sess) const
            {
                return operator()<typename Session::socket>(sess->socket_);
            }
        };

        struct dereference_equal
        {
            template<typename Session>
            bool operator()(const std::shared_ptr<Session>& lsess, 
                const std::shared_ptr<Session>& rsess) const
            {
                return *lsess == *rsess;
            }
        };

        template<
            typename Buffer = std::vector<char>,
            template<
                typename Value,
                typename = std::allocator<Value>
            > typename Container = std::deque
        > class sequence
        {
        public:
            using value_type = std::decay_t<Buffer>;
            using container = Container<value_type>;
            using iterator = typename container::iterator;
            using const_iterator = typename container::const_iterator;

            sequence() = delete;

            explicit sequence(boost::asio::io_context& context)
                : strand(context)
                , timer(context)
            {}

            explicit sequence(boost::asio::io_context::executor_type executor)
                : sequence(executor.context())
            {}

            bool is_empty() const noexcept
            {
                return std::empty(queue);
            }

            bool is_disposing() const noexcept
            {
                return is_disposing_;
            }

            void is_disposing(bool reverse_status) noexcept
            {
                core::verify(reverse_status ^ is_disposing_);
                is_disposing_ = reverse_status;
            }

            //Container<std::decay_t<Buffer>> queue;
            container queue;

            mutable boost::asio::io_context::strand strand;
            mutable boost::asio::steady_timer timer;

        private:
            bool is_disposing_ = false;
        };

    protected:
        template<typename Socket>
        static std::string endpoint_string(const Socket& sock)
        {
            std::ostringstream oss;
            oss << sock.local_endpoint() << sock.remote_endpoint();
            return oss.str();
        }
    };

    //using session_element = session<core::as_element_t>;

    template<typename Protocal>     
    class session<Protocal, boost::asio::basic_stream_socket>
        : public std::enable_shared_from_this<session<Protocal, boost::asio::basic_stream_socket>>
    {
    public:
        using protocal = std::decay_t<Protocal>;
        using element = session_element;
        using socket = boost::asio::basic_stream_socket<protocal>;

        session(socket&& sock, std::string_view delim)
            : socket_(std::move(sock))
            , recv_sequence_(socket_.get_executor())
            , send_sequence_(socket_.get_executor())
            , session_strand_(socket_.get_executor().context())
            , recv_delim_(delim)
            , hash_index_(element::dereference_hash{}.operator()<socket>(socket_))
        {
            core::verify(socket_.is_open());
            fmt::print(std::cout, "socket connected, {}/{}\n", socket_.local_endpoint(), socket_.remote_endpoint());
            if (!is_recv_delim_valid()) return;
            recv_sequence_.is_disposing(true);
            dispose_receive();
        }

        explicit session(socket&& socket)
            : session(std::move(socket), ""sv)
        {}

        session(const session&) = delete;

        session& operator=(const session&) = delete;

        ~session() = default;

        template<typename DynamicBuffer>
        std::future<void> receive(std::string_view delim, DynamicBuffer&& recv_buffer_view);

        [[nodiscard]] std::future<std::vector<char>> receive(std::string_view delim = ""sv)
        {
            std::promise<std::vector<char>> recv_promise;
            auto recv_future = recv_promise.get_future();
            pending_recv_request(std::move(recv_promise), delim);
            return recv_future;
        }

        void send(boost::asio::const_buffer send_buffer_view)
        {
            core::verify(send_buffer_view.size() != 0);
            pending_send_buffer_view(send_buffer_view);
        }

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
            return !(*this < that) && !(that < *this);
        }

    protected:
        template<typename Callable>
        boost::asio::executor_binder<std::decay_t<Callable>, boost::asio::io_context::strand>
            make_serial_handler(Callable&& handler)
        {
            return boost::asio::bind_executor(session_strand_, std::forward<Callable>(handler));
        }

        decltype(auto) make_fault_guard(const boost::system::error_code& error)
        {   //  displace verbose try-catch blocks
            return core::make_guard([&error, this]
            {
                const auto exception_count = std::uncaught_exceptions();
                if (!exception_count && !error) return;
                fmt::print(std::cerr, "fault detected\n");
                if (error) fmt::print(std::cerr, "error: {}\n", error.message());
                const auto socket_close_guard = core::make_guard([exception_count]
                {
                    if (std::uncaught_exceptions() > exception_count)
                        fmt::print(std::cerr, "exception detected during socket closing\n");
                });
                socket_.shutdown(decltype(socket_)::shutdown_both);
                socket_.close();
                fmt::print(std::cerr, "socket closed\n");
            });
        }

        void dispose_receive()
        {
            core::verify(recv_sequence_.is_disposing(), session_strand_.running_in_this_thread(), is_recv_delim_valid());
            auto& recv_queue = recv_sequence_.queue;
            const auto recv_buffer_iter = recv_queue.emplace(std::end(recv_queue));
            boost::asio::async_read_until(socket_, boost::asio::dynamic_buffer(*recv_buffer_iter), recv_delim_,
                make_serial_handler([recv_buffer_iter, this, self = this->shared_from_this()](
                    const boost::system::error_code& error, std::size_t transferred_size)
            {
                const auto guard = make_fault_guard(error);
                //core::verify(recv_buffer_iter->size() == transferred_size);
                fmt::print("handle: recv_buffer {}/ transferred {}\n", recv_buffer_iter->size(), transferred_size);
                if (!recv_request_.empty())
                {
                    //recv_request_.front().set_value(std::move(*recv_buffer_iter));
                    recv_request_.front().set_value(std::move(recv_sequence_.queue.front()));
                    recv_request_.pop_front();
                    //recv_sequence_.queue.erase(recv_buffer_iter);
                    recv_sequence_.queue.pop_front();
                }
                if (!error) dispose_receive();
            }));
        }

        void dispose_send()
        {
            core::verify(send_sequence_.is_disposing(), session_strand_.running_in_this_thread());
            auto& send_queue = send_sequence_.queue;
            if (send_queue.empty())
                return send_sequence_.is_disposing(false);
            const auto send_buffer_view = std::move(send_queue.front());    //  deque::iterator is unstable
            send_queue.pop_front();
            boost::asio::async_write(socket_, send_buffer_view,
                make_serial_handler([send_desired_size = send_buffer_view.size(), this, self = this->shared_from_this()](
                    const boost::system::error_code& error, std::size_t transferred_size)
            {
                const auto guard = make_fault_guard(error);
                core::verify(send_desired_size == transferred_size);
                if (!error) dispose_send();
            }));
        }

        void pending_recv_request(std::promise<std::vector<char>>&& recv_promise, std::string_view new_delim)
        {
            boost::asio::post(make_serial_handler([recv_promise = std::move(recv_promise),
                new_delim, this, self = this->shared_from_this()]() mutable
            {
                if (!new_delim.empty()) recv_delim_ = new_delim;
                if (!recv_request_.empty())             //  append after existed receiving requests
                    return recv_request_.push_back(std::move(recv_promise));
                if (!recv_sequence_.queue.empty())      //  is_disposing as well
                {                                       //  retrieve first available received buffer
                    recv_promise.set_value(std::move(recv_sequence_.queue.front()));
                    return recv_sequence_.queue.pop_front();
                }
                recv_request_.push_back(std::move(recv_promise));
                if (!recv_sequence_.is_disposing())
                {
                    core::verify(is_recv_delim_valid());
                    recv_sequence_.is_disposing(true);
                    dispose_receive();
                }
            }));
        }

        void pending_send_buffer_view(boost::asio::const_buffer send_buffer_view)
        {   
            boost::asio::post(make_serial_handler([send_buffer_view, this, self = this->shared_from_this()]
            {
                send_sequence_.queue.push_back(send_buffer_view);
                if (send_sequence_.is_disposing()) return;
                send_sequence_.is_disposing(true);
                dispose_send();
            }));
        }

        //element::socket<protocal> socket_;
        socket socket_;
        std::deque<std::promise<std::vector<char>>> recv_request_;
        element::sequence<std::vector<char>, std::list> recv_sequence_;
        element::sequence<boost::asio::const_buffer, std::deque> send_sequence_;

    private:
        bool is_index_valid() const noexcept
        {
            return hash_index_ != std::numeric_limits<size_t>::infinity();
        }

        bool is_recv_delim_valid() const noexcept
        {
            return !recv_delim_.empty();
        }

        mutable boost::asio::io_context::strand session_strand_;
        mutable std::string_view recv_delim_;

        const size_t hash_index_ = std::numeric_limits<size_t>::infinity();

        friend element::dereference_hash;
        friend element::dereference_equal;
    };
    
    template class session<boost::asio::ip::tcp>;
    //template class session<boost::asio::ip::udp>;
    //template class session<boost::asio::ip::icmp>;

    using tcp_session = session<boost::asio::ip::tcp>;
    using udp_session = session<boost::asio::ip::udp>;
}