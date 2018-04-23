#pragma once

namespace core
{
    // //  TODO: experimental
    // template<typename Callable>
    // struct dereference_callable
    // {
    //     template<typename ...Handles>
    //     decltype(auto) operator()(Handles&& ...args) const
    //     //    ->  std::invoke_result_t<std::decay_t<Callable>, decltype(*std::forward<Handles>(args))...>
    //     {
    //         return std::decay_t<Callable>{}((*std::forward<Handles>(args))...);
    //     }
    // };
}

// TODO: class core::storage, type-erasure by STL 
// TODO: class net::storage, type-erasure by ASIO buffer relavent utility

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

        template<typename Buffer,
            template<typename Elem, typename = std::allocator<Elem>> typename Container = std::deque>
        class sequence
        {
        public:
            sequence() = delete;

            explicit sequence(boost::asio::io_context& context)
                : strand(context)
                , timer(context)
            {}

            explicit sequence(boost::asio::io_context::executor_type executor)
                : sequence(executor.context())
            {}

            // struct invald_identity_error : std::logic_error
            // {
            //     using std::logic_error::logic_error;
            //     using std::logic_error::operator=;
            // };

            bool is_empty() const noexcept
            {
                return queue.empty();
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

            //std::deque<Buffer> queue;            
            Container<Buffer> queue;
            mutable boost::asio::io_context::strand strand;
            mutable boost::asio::steady_timer timer;

        private:
            bool is_disposing_ = false;
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
        : protected std::enable_shared_from_this<session<boost::asio::ip::tcp>>
    {
    public:
        session(boost::asio::ip::tcp::socket socket, std::string_view delim)
            : socket_(std::move(socket))
            , session_strand_(socket_.get_executor().context())
            , recv_delim_(delim)
            , recv_sequence_(socket_.get_executor())
            , send_sequence_(socket_.get_executor())
            , hash_index_(session_element::dereference_hash{}(socket_))
        {
            core::verify(socket.is_open());
            fmt::print(std::cout, "socket connected, {}/{}\n", socket.local_endpoint(), socket.remote_endpoint());
            if (!is_recv_delim_valid()) return;
            recv_sequence_.is_disposing(true);
            dispose_unserial_receive();
        }

        explicit session(boost::asio::ip::tcp::socket socket)
            : session(std::move(socket), ""sv)
        {}

        session(const session&) = delete;

        session& operator=(const session&) = delete;

        //virtual ~session() = default;

        template<typename DynamicBuffer>
        std::future<void> receive(std::string_view delim, DynamicBuffer&& recv_buffer_view);

        [[nodiscard]] std::future<std::vector<char>> receive(std::string_view delim = ""sv)
        {
            //if (!delim.empty()) recv_delim_ = delim;
            std::promise<std::vector<char>> recv_promise;
            auto recv_future = recv_promise.get_future();
            pending_before_receive(std::move(recv_promise), delim);
            return recv_future;
        }

        void send(boost::asio::const_buffer send_buffer_view)
        {
            core::verify(send_buffer_view.size() != 0);
            pending_before_send(send_buffer_view);
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
            return is_index_valid() && that.is_index_valid() ?
                hash_index_ == that.hash_index_ : !(*this < that) && !(that < *this);
        }

    protected:
        template<typename Callable>
        boost::asio::executor_binder<std::decay_t<Callable>, boost::asio::io_context::strand>
            make_serial_handler(Callable&& handler)
        {
            return boost::asio::bind_executor(session_strand_, std::forward<Callable>(handler));
        }

        decltype(auto) make_fault_guard(const boost::system::error_code& error)
        {   // displace verbose try-catch blocks
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

        void dispose_unserial_receive();

        void dispose_receive(std::promise<std::vector<char>>&& recv_promise,
            std::list<std::vector<char>>::iterator recv_buffer_iter)
        {
            //core::verify(recv_sequence_.is_disposing(), session_strand_.running_in_this_thread() || !is_recv_delim_valid());
        
        }

        void dispose_send()
        {
            core::verify(send_sequence_.is_disposing(), session_strand_.running_in_this_thread());
            auto& queue_to_write = send_sequence_.queue;
            if (queue_to_write.empty())
                return send_sequence_.is_disposing(false);
            const auto buffer_view_to_write = std::move(queue_to_write.front());
            queue_to_write.pop_front();
            async_write(socket_, buffer_view_to_write,
                make_serial_handler([self = shared_from_this(), expired_size = buffer_view_to_write.size()](
                    boost::system::error_code error, std::size_t transferred_size)
            {
                const auto guard = self->make_fault_guard(error);
                core::verify(expired_size == transferred_size);
                if (!error) self->dispose_send();
            }));
        }

        void pending_before_receive(std::promise<std::vector<char>>&& recv_promise, std::string_view new_delim)
        {
            post(make_serial_handler([recv_promise = std::move(recv_promise),
                new_delim, self = shared_from_this()]() mutable noexcept
            {
                if (!new_delim.empty()) self->recv_delim_ = new_delim;
                auto& queue_to_recv = self->recv_sequence_.queue;
                queue_to_recv.emplace_back();
                if (self->recv_sequence_.is_disposing()) return;
                //auto&& recv_buffer_view = boost::asio::dynamic_buffer(self->recv_sequence_.queue.back());
                self->recv_sequence_.is_disposing(true);
                self->dispose_receive(std::move(recv_promise), std::prev(queue_to_recv.end()));
            }));
        }

        void pending_before_send(boost::asio::const_buffer write_buffer_view)
        {
            post(make_serial_handler([write_buffer_view, self = shared_from_this()]()
            {
                self->send_sequence_.queue.push_back(write_buffer_view);
                if (self->send_sequence_.is_disposing()) return;
                self->send_sequence_.is_disposing(true);
                self->dispose_send();
            }));
        }

        boost::asio::ip::tcp::socket socket_;
        session_element::sequence<std::vector<char>, std::list> recv_sequence_;
        session_element::sequence<boost::asio::const_buffer> send_sequence_;

    private:
        const boost::asio::ip::tcp::socket& socket() const noexcept
        { 
            return socket_;
        }

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

        friend session_element::dereference_hash;
        friend session_element::dereference_equal;
    };
    
    //  TODO:  template<> class session<boost::asio::ip::udp>;

    //template class session<boost::asio::ip::tcp>;
    //template class session<boost::asio::ip::udp>;

    using tcp_session = session<boost::asio::ip::tcp>;
    //using udp_session = session<boost::asio::ip::udp>;
}
