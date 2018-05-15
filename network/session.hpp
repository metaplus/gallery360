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
            , delim_suffix_(delim)
            , session_strand_(socket_.get_executor().context())
            , recv_actor_strand_(socket_.get_executor().context())
            , hash_code_(element::dereference_hash{}.operator()<socket>(socket_))
        {
            core::verify(socket_.is_open());
            fmt::print(std::cout, "socket connected, {}/{}\n", socket_.local_endpoint(), socket_.remote_endpoint());
            // if (!delim_suffix_valid()) return;
            // recv_disposing_ = true;
            // dispose_receive();
        }

        explicit session(socket&& socket)
            : session(std::move(socket), ""sv)
        {}

        session(const session&) = delete;

        session& operator=(const session&) = delete;

        ~session() = default;

        std::shared_mutex& external_recvbuf()
        {
            assert(recv_actor_.external == nullptr);
            recv_actor_.external = std::make_unique<recv_actor::external_facet>();
            return recv_actor_.external->streambuf_mutex;
        }

        std::string_view recv_delim() const noexcept
        {
            return delim_suffix_;
        }

        std::string recv_delim_suffix(std::string_view new_suffix) noexcept
        {
            return std::exchange(delim_suffix_, new_suffix);
        }

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
            //core::verify(sendbuf_view.size() != 0);
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

        void close_socket(core::defer_execute_t)
        {
            boost::asio::dispatch(session_strand_, [this, self = shared_from_this()]()
            {
                close_pending_ = true;
                if (!finish_send()) return;
                boost::asio::dispatch(recv_actor_strand_, [this, self = std::move(self)]()
                {
                    assert(session_strand_.running_in_this_thread());
                    if (finish_receive()) dispose_close_socket();
                });
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
        decltype(auto) make_fault_guard(const boost::system::error_code& error)
        {   //  displace verbose try-catch blocks
            return core::make_guard([&error, this]
            {
                const auto exception_count = std::uncaught_exceptions();
                if (!exception_count && !error) return;
                fmt::print(std::cerr, "fault guard triggered\n");
                if (error) fmt::print(std::cerr, "error: {}\n" "connection closed by peer: {}\n",
                    error.message(), error == boost::asio::error::eof);
                const auto socket_close_guard = core::make_guard([exception_count]
                {
                    if (std::uncaught_exceptions() > exception_count)
                        fmt::print(std::cerr, "exception detected during socket closing\n");
                });
                close_socket(core::defer_execute);
                fmt::print(std::cerr, "socket closed\n");
            });
        }

        using std::enable_shared_from_this<session<Protocal, boost::asio::basic_stream_socket>>::shared_from_this;

        using std::enable_shared_from_this<session<Protocal, boost::asio::basic_stream_socket>>::weak_from_this;

        void dispose_receive()
        {
            assert(recv_disposing_);
            assert(session_strand_.running_in_this_thread());
            assert(delim_suffix_valid());
            assert(delim_suffix_.size() <= std::numeric_limits<uint16_t>::max());
            const auto& recv_delim_ref = generate_delim(recv_delim_index_);
            boost::asio::async_read_until(socket_, recv_actor_.streambuf, recv_delim_ref,
                boost::asio::bind_executor(session_strand_, [delim_size = static_cast<uint16_t>(recv_delim_ref.size()),
                    this, self = shared_from_this()](boost::system::error_code error, std::size_t transfer_size) mutable
            {
                if (delim_size == transfer_size) fmt::print("empty net pack received\n");
                const auto guard = make_fault_guard(error);
                const auto consume_size = std::atomic_exchange(&recvbuf_consume_size_, 0);
                if (consume_size > 0) recv_actor_.streambuf.consume(consume_size);
                fmt::print(">>handle: transferred {} ", transfer_size - consume_size);
                boost::asio::post(recv_actor_strand_,
                    [delim_size, transfer_size = transfer_size - consume_size, this, self]() mutable
                {
                    if (!recv_actor_.has_request()) return;
                    reply_recv_request(delim_size, transfer_size);
                });
                // if (streambuf_lock.owns_lock()) streambuf_lock.unlock();
                if (!error && transfer_size != 0 && delim_size < transfer_size) return dispose_receive();
                recv_disposing_ = false;
            }));
        }

        void dispose_send()
        {
            assert(send_disposing_);
            assert(session_strand_.running_in_this_thread());
            assert(delim_suffix_valid());
            if (finish_send())
            {
                fmt::print("stop dispose send\n");
                if (close_pending_) boost::asio::dispatch(recv_actor_strand_, 
                    [this, self = shared_from_this()]{ if (finish_receive()) dispose_close_socket(); });
                send_disposing_ = false;
                return;
            }
            auto[sendbuf_view, sendbuf_handle] = std::move(sendbuf_handles_.front());   //  deque::iterator is unstable
            sendbuf_handles_.pop_front();
            std::array<boost::asio::const_buffer, 2> sendbuf_delim_view{ sendbuf_view ,
                boost::asio::buffer(generate_delim(send_delim_index_)) };
            boost::asio::async_write(socket_, sendbuf_delim_view,
                boost::asio::bind_executor(session_strand_, [expect_size = buffer_size(sendbuf_delim_view), sendbuf_handle = std::move(sendbuf_handle),
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

        [[nodiscard]] std::future<std::vector<char>> 
            pending_recv_request(std::promise<std::vector<char>>&& recv_promise, std::string_view new_delim)
        {
            auto recv_future = recv_promise.get_future();
            boost::asio::post(recv_actor_strand_,
                [recv_promise = std::move(recv_promise), new_delim, this, self = shared_from_this()]() mutable
            {
                //[[maybe_unused]] auto& recv_actor = try_construct_recv_actor<internal_recv_actor>();    
                assert(!delim_suffix_.empty());
                // if (!new_delim.empty() && new_delim != delim_suffix_) delim_suffix_ = new_delim;
                recv_actor_.requests.push_back(std::move(recv_promise));
                if (recv_actor_.is_streambuf_available()) return reply_recv_request();
                boost::asio::dispatch(session_strand_,
                    [recv_promise = std::move(recv_promise), this, self = std::move(self)]() mutable
                {
                    if (close_pending_ || !socket_.is_open())
                        return recv_actor_.clear_request();
                    if (std::exchange(recv_disposing_, true)) return;
                    assert(delim_suffix_valid());
                    dispose_receive();
                });
            });
            return recv_future;
        }


        void pending_sendbuf_handle(boost::asio::const_buffer sendbuf_view, std::unique_ptr<std::any> sendbuf_handle)
        {   
            boost::asio::post(session_strand_, 
                [sendbuf_view, sendbuf_handle = std::move(sendbuf_handle), this, self = shared_from_this()]() mutable
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
                // close_pending_ = false;
                recv_disposing_ = false;
                send_disposing_ = false;
                // socket_.cancel();
                if (!socket_.is_open()) return;
                socket_.shutdown(socket::shutdown_both);
                socket_.close();
            });
        }

        struct recv_actor
        {
            struct external_facet
            {
                boost::asio::streambuf streambuf;
                std::shared_mutex streambuf_mutex;
                std::optional<std::promise<std::unique_lock<std::shared_mutex>>> request;
            };

            bool is_streambuf_available() const noexcept
            {
                return !streambuf_infos.empty();
            }

            bool has_request() const noexcept
            {
                return !requests.empty() || external != nullptr && external->request.has_value();
            }

            std::pair<uint16_t, uint64_t> rotate_streambuf_info(uint16_t delim_size, uint64_t transfer_size)
            {
                if (streambuf_infos.empty()) return std::make_pair(delim_size, transfer_size);
                streambuf_infos.emplace_back(delim_size, transfer_size);
                return rotate_streambuf_info();
            }

            std::pair<uint16_t, uint64_t> rotate_streambuf_info()
            {
                assert(!streambuf_infos.empty());
                const auto front_info = streambuf_infos.front();
                streambuf_infos.pop_front();
                return front_info;
            }

            std::tuple<uint64_t, uint64_t, bool> reply_request()
            {
                assert(has_request());
                // assert(is_streambuf_available());
                const auto[delim_size, transfer_size] = rotate_streambuf_info();
                return dispose_reply_request(delim_size, transfer_size);
            }

            std::tuple<uint64_t, uint64_t, bool> reply_request(uint16_t delim_size, uint64_t transfer_size)
            {
                assert(has_request());
                // assert(is_streambuf_available());
                std::tie(delim_size, transfer_size) = rotate_streambuf_info(delim_size, transfer_size);
                return dispose_reply_request(delim_size, transfer_size);
            }

            std::tuple<uint64_t, uint64_t, bool> dispose_reply_request(uint16_t delim_size, uint64_t transfer_size)
            {
                std::tuple<uint64_t, uint64_t, bool> reply_context{ 0,0,false };
                if (delim_size < transfer_size && transfer_size != 0)
                {
                    if (!requests.empty())
                    {
                        std::vector<char> streambuf_front(transfer_size - delim_size);
                        const auto copy_size = buffer_copy(boost::asio::buffer(streambuf_front), streambuf.data());
                        assert(copy_size == streambuf_front.size());
                        std::get<0>(reply_context) = copy_size;
                        requests.front().set_value(std::move(streambuf_front));
                        requests.pop_front();
                    }
                    if (external != nullptr)
                    {
                        const auto copy_size = boost::asio::buffer_copy(
                            external->streambuf.prepare(transfer_size - delim_size), streambuf.data());
                        std::get<1>(reply_context) = copy_size;
                        external->streambuf.commit(copy_size);
                        if (external->request.has_value())
                        {
                            external->request->set_value(std::unique_lock<std::shared_mutex>{external->streambuf_mutex});
                            std::get<2>(reply_context) = true;
                            external->request.reset();              //  optional::reset
                        }
                    }
                }
                else clear_request();
                return reply_context;
            }

            void clear_request()
            {
                if (!requests.empty())
                {
                    for (auto& promise : requests) promise.set_value(std::vector<char>{});
                    requests.clear();
                }
                if (external != nullptr && external->request.has_value())
                {
                    external->request->set_value(std::unique_lock<std::shared_mutex>{external->streambuf_mutex});
                    external->request.reset();
                }
            }

            boost::asio::streambuf streambuf;
            std::deque<std::pair<uint16_t, uint64_t>> streambuf_infos;
            std::deque<std::promise<std::vector<char>>> requests;
            std::unique_ptr<external_facet> external;
        };

        static_assert(std::is_default_constructible_v<recv_actor>);

        socket socket_;
        recv_actor recv_actor_;
        std::string delim_suffix_;
        std::deque<std::pair<boost::asio::const_buffer, std::unique_ptr<std::any>>> sendbuf_handles_;
        mutable bool close_pending_ = false;
        mutable bool recv_disposing_ = false;
        mutable bool send_disposing_ = false;

    private:
        bool hash_code_valid() const noexcept
        {
            return hash_code_ != std::numeric_limits<size_t>::infinity();
        }

        bool delim_suffix_valid() const noexcept
        {
            return !delim_suffix_.empty();
        }

        bool finish_receive() const noexcept
        {
            assert(recv_actor_strand_.running_in_this_thread());
            //return !has_recv_request() && !has_available_recvbuf();
            return !recv_actor_.has_request() && !recv_actor_.is_streambuf_available();
        }

        bool finish_send() const noexcept
        {
            return sendbuf_handles_.empty();
        }

#pragma warning(push)
#pragma warning(disable: 4101)
        void reply_recv_request(uint16_t delim_size = 0, uint64_t transfer_size = 0)
        {
            assert(recv_actor_strand_.running_in_this_thread());
            assert(recv_actor_.has_request());
            assert(recv_actor_.is_streambuf_available() || delim_size != 0 && transfer_size != 0);
            const auto[vector_size, streambuf_size, has_external_request] = recv_actor_.reply_request(delim_size, transfer_size);
            const auto expect_size = transfer_size - delim_size;
            assert(vector_size == expect_size || streambuf_size == expect_size);
            std::atomic_fetch_add(&recvbuf_consume_size_, transfer_size);
        }
#pragma warning(pop)

        const std::string& generate_delim(std::atomic<uint64_t>& prefix_index) const
        {
            assert(&prefix_index == &recv_delim_index_ || &prefix_index == &send_delim_index_);
            auto& packed_delim = &prefix_index == &recv_delim_index_ ? recv_delim_ : send_delim_;
            const auto delim_index = prefix_index.fetch_add(1);
            packed_delim.clear();
            packed_delim.reserve(delim_prefix_.size() + sizeof delim_index + delim_suffix_.size());
            return packed_delim
                .append(delim_prefix_)          //  Cautious: processor-dependent endianness
                .append(reinterpret_cast<const char*>(&delim_index), sizeof delim_index)
                .append(delim_suffix_);
        }

        mutable boost::asio::io_context::strand session_strand_;
        mutable boost::asio::io_context::strand recv_actor_strand_;
        mutable std::atomic<uint64_t> recvbuf_consume_size_ = 0;
        mutable std::atomic<uint64_t> recv_delim_index_ = 0;
        mutable std::atomic<uint64_t> send_delim_index_ = 0;
        mutable std::string recv_delim_;
        mutable std::string send_delim_;

        const std::string delim_prefix_ = "[PacketDelim]"s;
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