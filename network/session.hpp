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
            if (!recv_delim_valid()) return;
            recv_disposing_ = true;
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

        void send(boost::asio::const_buffer sendbuf_view)
        {
            core::verify(sendbuf_view.size() != 0);
            pending_sendbuf_handle(sendbuf_view, nullptr);
        }

        template<typename AnyType>
        void send(boost::asio::const_buffer sendbuf_view, AnyType&& sendbuf_host)
        {
            core::verify(sendbuf_view.size() != 0);
            std::unique_ptr<std::any> sendbuf_handle = std::make_unique<std::any>(std::forward<AnyType>(sendbuf_host));
            pending_sendbuf_handle(sendbuf_view, std::move(sendbuf_handle));
        }

        bool operator<(const session& that) const
        {
            return hash_code_valid() && that.hash_code_valid() ?
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

        void close_socket_finally()
        {
            boost::asio::dispatch(session_strand_, [this, self = shared_from_this()]()
            {
                close_pending_ = true;
                if (finish_receive() && finish_send()) dispose_close_socket();
            });
        }

        size_t hash_code() const noexcept
        {
            assert(hash_code_valid());
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
        using promise_bufvec = std::promise<std::vector<char>>;
        using promise_bufinfo = std::promise<std::pair<uint16_t, uint64_t>>;
        using promise_streambuf = std::pair<promise_bufinfo, boost::asio::streambuf&>;

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
            assert(recv_disposing_);
            assert(session_strand_.running_in_this_thread());
            assert(recv_delim_valid());
            assert(recv_delim_.size() <= std::numeric_limits<uint16_t>::max());
            boost::asio::async_read_until(socket_, ref_recv_streambuf(), recv_delim_,
                make_serial_handler([delim_size = static_cast<uint16_t>(recv_delim_.size()),
                    this, self = shared_from_this()](boost::system::error_code error, std::size_t transfer_size)
            {
                if (delim_size == transfer_size)
                    fmt::print("empty net pack received\n");
                const auto guard = make_fault_guard(error);
                fmt::print(">>handle: transferred {}\n", transfer_size);
                if (!recv_request_empty())
                {
                    reply_recv_request(delim_size, transfer_size);
                    // if (!recv_streambuf_infos_.empty())
                    // {
                    //     recv_requests_.front().set_value(pop_recv_streambuf_front());
                    //     recv_streambuf_infos_.emplace_back(delim_size, transfer_size);
                    // }
                    // else
                    //     recv_requests_.front().set_value(pop_recv_streambuf_front(delim_size, transfer_size));
                    // recv_requests_.pop_front();
                }
                if (!error && delim_size != transfer_size) return dispose_receive();
                recv_disposing_ = false;
            }));
        }

        void dispose_send()
        {
            assert(send_disposing_);
            assert(session_strand_.running_in_this_thread());
            // if (send_queue.empty())    
            if (finish_send())
            {
                fmt::print("stop dispose send\n");
                if (close_pending_ && finish_receive()) dispose_close_socket();
                send_disposing_ = false;
                return;
            }
            auto[sendbuf_view, sendbuf_handle] = std::move(sendbuf_handles_.front());   //  deque::iterator is unstable
            sendbuf_handles_.pop_front();
            boost::asio::async_write(socket_, sendbuf_view,
                make_serial_handler([expect_size = sendbuf_view.size(), sendbuf_handle = std::move(sendbuf_handle), 
                    this, self = shared_from_this()](boost::system::error_code error, std::size_t transfer_size) 
            {
                const auto guard = make_fault_guard(error);
                fmt::print(">>handle: transferred {}, next to transfer {}, queue size {}\n", transfer_size,
                    sendbuf_handles_.empty() ? 0 : sendbuf_handles_.front().first.size(), sendbuf_handles_.size());
                assert(expect_size == transfer_size);
                if (sendbuf_handle) sendbuf_handle->reset();                            //  any::reset()
                if (!error && expect_size == transfer_size) return dispose_send();
                fmt::print(">>handle: stop dispose send\n");
                dispose_close_socket();
                send_disposing_ = false;
            }));
        }

        [[nodiscard]] std::future<std::vector<char>> pending_recv_request(
            std::promise<std::vector<char>>&& recv_promise, std::string_view new_delim)
        {
            auto recv_future = recv_promise.get_future();
            boost::asio::post(session_strand_, [recv_promise = std::move(recv_promise),
                new_delim, this, self = shared_from_this()]() mutable
            {
                if (!recv_actor_constructed())
                    recv_actor_.template emplace<internal_recv_actor>();
                if (!new_delim.empty() && new_delim != recv_delim_) recv_delim_ = new_delim;
                auto& recv_requests = std::get<internal_recv_actor>(recv_actor_).requests;
                if (!recv_requests.empty())                //  append after existed receiving requests
                    return recv_requests.push_back(std::move(recv_promise));
                if (!recv_streambuf_infos_.empty())         //  retrieve front available received bytes
                    return recv_promise.set_value(pop_recv_streambuf_front());
                recv_requests.push_back(std::move(recv_promise));
                if (std::exchange(recv_disposing_, true)) return;
                assert(recv_delim_valid());
                dispose_receive();
            });
            return recv_future;
        }

        void pending_sendbuf_handle(boost::asio::const_buffer sendbuf_view, std::unique_ptr<std::any> sendbuf_handle)
        {   
            boost::asio::post(session_strand_, [sendbuf_view, 
                sendbuf_handle = std::move(sendbuf_handle), this, self = shared_from_this()]() mutable
            {
                sendbuf_handles_.emplace_back(sendbuf_view,std::move(sendbuf_handle));
                if (std::exchange(send_disposing_, true)) return;
                dispose_send();
            });
        }

        void dispose_close_socket()
        {
            boost::asio::dispatch(session_strand_, [this, self = shared_from_this()]()
            {
                // socket_.cancel();
                socket_.shutdown(socket::shutdown_both);
                socket_.close();
            });
        }

        struct internal_recv_actor
        {
            std::deque<std::promise<std::vector<char>>> requests;
            boost::asio::streambuf streambuf;
        };

        struct external_recv_actor
        {
            std::deque<std::promise<std::pair<uint16_t, uint64_t>>> requests;
            boost::asio::streambuf& streambuf;
        };

        socket socket_;
        std::variant<std::monostate, internal_recv_actor, external_recv_actor> recv_actor_;
        // boost::asio::streambuf recv_streambuf_;
        // std::deque<std::promise<std::vector<char>>> recv_requests_;
        std::deque<std::pair<uint16_t, uint64_t>> recv_streambuf_infos_;
        std::deque<std::pair<boost::asio::const_buffer, std::unique_ptr<std::any>>> sendbuf_handles_;
        mutable bool close_pending_ = false;
        mutable bool recv_disposing_ = false;
        mutable bool send_disposing_ = false;

    private:
        bool hash_code_valid() const noexcept
        {
            return hash_code_ != std::numeric_limits<size_t>::infinity();
        }

        bool recv_delim_valid() const noexcept
        {
            return !recv_delim_.empty();
        }

        bool finish_receive() const noexcept
        {
            return recv_request_empty() && recv_streambuf_infos_.empty();
            // return recv_requests_.empty() && recv_streambuf_infos_.empty();
        }

        bool finish_send() const noexcept
        {
            return sendbuf_handles_.empty();
        }

        std::vector<char> pop_recv_streambuf_front(uint16_t delim_size, uint64_t transferred_size)
        {
            auto& recv_streambuf = std::get<internal_recv_actor>(recv_actor_).streambuf;
            assert(delim_size <= transferred_size);
            assert(recv_streambuf.size() >= transferred_size);
            assert(session_strand_.running_in_this_thread());
            const auto recv_streambuf_iter = buffers_begin(recv_streambuf.data());
            std::vector<char> recv_streambuf_front{ recv_streambuf_iter,std::next(recv_streambuf_iter,transferred_size - delim_size) };
            recv_streambuf.consume(transferred_size);
            return recv_streambuf_front;
        }

        std::vector<char> pop_recv_streambuf_front()
        {
            assert(!recv_streambuf_infos_.empty());
            const auto[delim_size, transfer_size] = std::move(recv_streambuf_infos_.front());
            recv_streambuf_infos_.pop_front();
            return pop_recv_streambuf_front(delim_size, transfer_size);
        }

        bool recv_actor_constructed() const
        {
            static_assert(std::is_same_v<std::monostate, std::variant_alternative_t<0, decltype(recv_actor_)>>);
            return recv_actor_.index() != 0;
        }

        bool recv_request_empty() const
        {
            if (!recv_actor_constructed()) return false;
            if (const internal_recv_actor* internal_actor = std::get_if<internal_recv_actor>(&recv_actor_))
                return internal_actor->requests.empty();
            if (const external_recv_actor* external_actor = std::get_if<external_recv_actor>(&recv_actor_))
                return external_actor->requests.empty();
            throw core::unreachable_execution_branch{};
        }

        boost::asio::streambuf& ref_recv_streambuf()
        {
            if (internal_recv_actor* actor = std::get_if<internal_recv_actor>(&recv_actor_))
                return actor->streambuf;
            if (external_recv_actor* actor = std::get_if<external_recv_actor>(&recv_actor_))
                return actor->streambuf;
            throw core::unreachable_execution_branch{};
        }

        void reply_recv_request(uint16_t delim_size, uint64_t transfer_size)
        {
            if (internal_recv_actor* actor = std::get_if<internal_recv_actor>(&recv_actor_))
            {
                assert(!actor->requests.empty());
                if (!recv_streambuf_infos_.empty())
                {
                    actor->requests.front().set_value(pop_recv_streambuf_front());
                    recv_streambuf_infos_.emplace_back(delim_size, transfer_size);
                }
                else
                    actor->requests.front().set_value(pop_recv_streambuf_front(delim_size, transfer_size));
                return actor->requests.pop_front();
            }
            if (external_recv_actor* actor = std::get_if<external_recv_actor>(&recv_actor_))
            {
                assert(!actor->requests.empty());
                actor->requests.front().set_value(std::make_pair(delim_size, transfer_size));
                return actor->requests.pop_front();
            }
            throw core::unreachable_execution_branch{};
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