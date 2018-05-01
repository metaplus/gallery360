#pragma once

namespace net
{
    template<
        typename Protocal = boost::asio::ip::tcp,
        template<typename> typename Socket = boost::asio::basic_stream_socket> 
    class session;

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
            , session_strand_(socket_.get_executor().context())
            , recv_delim_(delim)
            , hash_code_(element::dereference_hash{}.operator()<socket>(socket_))
        {
            core::verify(socket_.is_open());
            fmt::print(std::cout, "socket connected, {}/{}\n", socket_.local_endpoint(), socket_.remote_endpoint());
            if (!is_recv_delim_valid()) return;
            recv_is_disposing_ = true;
            dispose_receive();
        }

        explicit session(socket&& socket)
            : session(std::move(socket), ""sv)
        {}

        session(const session&) = delete;

        session& operator=(const session&) = delete;

        ~session() = default;

        [[nodiscard]] std::future<std::vector<char>> receive(core::use_future_t, std::string_view delim = ""sv)
        {
            std::promise<std::vector<char>> recv_promise;
            return pending_recv_request(std::move(recv_promise), delim);
        }

        std::vector<char> receive(std::string_view delim = ""sv)
        {
            return receive(core::use_future, delim).get();
        }

        void send(boost::asio::const_buffer send_bufview)
        {
            core::verify(send_bufview.size() != 0);
            pending_send_bufview(send_bufview, nullptr);
        }

        template<typename AnyType>
        void send(boost::asio::const_buffer send_bufview, AnyType&& send_bufhost)
        {
            core::verify(send_bufview.size() != 0);
            std::unique_ptr<std::any> send_bufhandle = std::make_unique<std::any>(std::forward<AnyType>(send_bufhost));
            pending_send_bufview(send_bufview, std::move(send_bufhandle));
        }

        bool operator<(const session& that) const
        {
            return is_hash_code_valid() && that.is_hash_code_valid() ?
                hash_code() < that.hash_code() :
                socket_.local_endpoint() < that.socket_.local_endpoint()
                || !(that.socket_.local_endpoint() < socket_.local_endpoint())
                && socket_.remote_endpoint() < that.socket_.remote_endpoint();
        }

        bool operator==(const session& that) const
        {
            return !(*this < that) && !(that < *this);
        }

        void close_socket()
        {
            dispose_close_socket();
        }

        void close_socket_after_finish()
        {
            boost::asio::dispatch(session_strand_, [this, self = shared_from_this()]()
            {
                pending_close_ = true;
                if (is_recv_finish() && is_send_finish()) dispose_close_socket();
            });
        }

        size_t hash_code() const noexcept
        {
            assert(is_hash_code_valid());
            return hash_code_;
        }

        struct index : element::session_index
        {
            using session_index::session_index;
            using session_index::operator=;
            using hash = default_hash;
        };

        static_assert(std::is_convertible_v<index, element::session_index>);
        static_assert(!std::is_convertible_v<element::session_index, index>);
        static_assert(sizeof index == sizeof element::session_index);

        index hash_index() const noexcept
        {
            return index{ hash_code() };
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
                fmt::print(std::cerr, "fault guard triggered\n");
                if (error) fmt::print(std::cerr, "error: {}, connection is connected by peer: {}\n", 
                    error.message(), error == boost::asio::error::eof);
                const auto socket_close_guard = core::make_guard([exception_count]
                {
                    if (std::uncaught_exceptions() > exception_count)
                        fmt::print(std::cerr, "exception detected during socket closing\n");
                });
                close_socket();
                fmt::print(std::cerr, "socket closed\n");
            });
        }

        using std::enable_shared_from_this<session<Protocal, boost::asio::basic_stream_socket>>::shared_from_this;

        using std::enable_shared_from_this<session<Protocal, boost::asio::basic_stream_socket>>::weak_from_this;

        void dispose_receive()
        {
            assert(recv_is_disposing_);
            assert(session_strand_.running_in_this_thread());
            assert(is_recv_delim_valid());
            boost::asio::async_read_until(socket_, recv_streambuf_, recv_delim_,
                make_serial_handler([delim_size = static_cast<uint16_t>(recv_delim_.size()),
                    this, self = shared_from_this()](boost::system::error_code error, std::size_t transfer_size)
            {
                if (delim_size == transfer_size)
                    fmt::print("empty net pack received\n");
                const auto guard = make_fault_guard(error);
                fmt::print(">>handle: transferred {}\n", transfer_size);
                if (!recv_requests_.empty())
                {
                    if (!recv_streambuf_infos_.empty())
                    {
                        recv_requests_.front().set_value(pop_recv_streambuf_front());
                        recv_streambuf_infos_.emplace_back(delim_size, transfer_size);
                    }
                    else
                        recv_requests_.front().set_value(pop_recv_streambuf_front(delim_size, transfer_size));
                    recv_requests_.pop_front();
                }
                if (!error && delim_size != transfer_size) dispose_receive();
                else recv_is_disposing_ = false;
            }));
        }

        void dispose_send()
        {
            assert(send_is_disposing_);
            assert(session_strand_.running_in_this_thread());
            auto& send_queue = send_bufhandles_;
            // if (send_queue.empty())    
            if (is_send_finish())
            {
                fmt::print("stop dispose send\n");
                if (pending_close_ && is_recv_finish()) dispose_close_socket();
                send_is_disposing_ = false;
                return;
            }
            auto[send_bufview, send_bufhandle] = std::move(send_queue.front());    //  deque::iterator is unstable
            send_queue.pop_front();
            boost::asio::async_write(socket_, send_bufview,
                make_serial_handler([expect_size = send_bufview.size(), send_bufhandle = std::move(send_bufhandle), 
                    this, self = shared_from_this()](boost::system::error_code error, std::size_t transfer_size) 
            {
                const auto guard = make_fault_guard(error);
                fmt::print(">>handle: transferred {}, next to transfer {}, queue size {}\n", transfer_size,
                    send_bufhandles_.empty() ? 0 : send_bufhandles_.front().first.size(), send_bufhandles_.size());
                if (expect_size != transfer_size)
                    auto dummy = 1;
                assert(expect_size == transfer_size);
                if (send_bufhandle) send_bufhandle->reset();
                if (!error && expect_size == transfer_size) dispose_send();
                else
                {
                    fmt::print(">>handle: stop dispose send\n");
                    dispose_close_socket();
                    send_is_disposing_ = false;
                }
            }));
        }

        [[nodiscard]] std::future<std::vector<char>> pending_recv_request(
            std::promise<std::vector<char>>&& recv_promise, std::string_view new_delim)
        {
            auto recv_future = recv_promise.get_future();
            boost::asio::post(session_strand_, [recv_promise = std::move(recv_promise),
                new_delim, this, self = shared_from_this()]() mutable
            {
                if (!new_delim.empty() && new_delim != recv_delim_) recv_delim_ = new_delim;
                if (!recv_requests_.empty())                //  append after existed receiving requests
                    return recv_requests_.push_back(std::move(recv_promise));
                if (!recv_streambuf_infos_.empty())   //  retrieve front available received bytes
                    return recv_promise.set_value(pop_recv_streambuf_front());
                recv_requests_.push_back(std::move(recv_promise));
                if (std::exchange(recv_is_disposing_, true)) return;
                assert(is_recv_delim_valid());
                dispose_receive();
            });
            return recv_future;
        }

        void pending_send_bufview(boost::asio::const_buffer send_bufview, std::unique_ptr<std::any> send_bufhandle)
        {   
            boost::asio::post(session_strand_, [send_bufview, 
                send_bufhandle = std::move(send_bufhandle), this, self = shared_from_this()]() mutable
            {
                send_bufhandles_.emplace_back(send_bufview,std::move(send_bufhandle));
                if (std::exchange(send_is_disposing_, true)) return;
                dispose_send();
            });
        }

        void dispose_close_socket()
        {
            boost::asio::dispatch(session_strand_, [this, self = shared_from_this()]()
            {
                //socket_.cancel();
                socket_.shutdown(socket::shutdown_both);
                socket_.close();
            });
        }

        socket socket_;
        boost::asio::streambuf recv_streambuf_;
        std::deque<std::promise<std::vector<char>>> recv_requests_;
        std::deque<std::pair<uint16_t, uint64_t>> recv_streambuf_infos_;
        std::deque<std::pair<boost::asio::const_buffer, std::unique_ptr<std::any>>> send_bufhandles_;
        mutable bool pending_close_ = false;
        mutable bool recv_is_disposing_ = false;
        mutable bool send_is_disposing_ = false;

    private:
        bool is_hash_code_valid() const noexcept
        {
            return hash_code_ != std::numeric_limits<size_t>::infinity();
        }

        bool is_recv_delim_valid() const noexcept
        {
            return !recv_delim_.empty();
        }

        bool is_recv_finish() const noexcept
        {
            return recv_requests_.empty() && recv_streambuf_infos_.empty();
        }

        bool is_send_finish() const noexcept
        {
            return send_bufhandles_.empty();
        }

        std::vector<char> pop_recv_streambuf_front(uint16_t delim_size, uint64_t transferred_size)
        {
            assert(delim_size <= transferred_size);
            assert(recv_streambuf_.size() >= transferred_size);
            assert(session_strand_.running_in_this_thread());
            const auto recv_streambuf_iter = buffers_begin(recv_streambuf_.data());
            std::vector<char> recv_streambuf_front{ recv_streambuf_iter,std::next(recv_streambuf_iter,transferred_size - delim_size) };
            recv_streambuf_.consume(transferred_size);
            return recv_streambuf_front;
        }

        std::vector<char> pop_recv_streambuf_front()
        {
            assert(!recv_streambuf_infos_.empty());
            const auto[delim_size, transferred_size] = std::move(recv_streambuf_infos_.front());
            recv_streambuf_infos_.pop_front();
            return pop_recv_streambuf_front(delim_size, transferred_size);
        }

        mutable boost::asio::io_context::strand session_strand_;
        mutable std::string_view recv_delim_;
        const size_t hash_code_ = std::numeric_limits<size_t>::infinity();

        friend element::dereference_hash;
        friend element::dereference_equal;
    };

    template class session<>;
    // template class session<boost::asio::ip::tcp, boost::asio::basic_stream_socket>;
    // template class session<boost::asio::ip::udp, boost::asio::basic_datagram_socket>;
    // template class session<boost::asio::ip::icmp, boost::asio::basic_raw_socket>;

    using default_session = session<>;
    using tcp_session = session<boost::asio::ip::tcp, boost::asio::basic_stream_socket>;
    using udp_session = session<boost::asio::ip::udp, boost::asio::basic_datagram_socket>;
}