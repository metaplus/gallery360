#pragma once

namespace net
{
    namespace base
    {
        template<typename Socket, typename DynamicBuffer = boost::asio::streambuf>
        class recv_base;

        template<>
        class recv_base<boost::asio::ip::tcp::socket, boost::asio::streambuf> : context_base
        {
        protected:
            recv_base(boost::asio::ip::tcp::socket& sock, boost::asio::io_context::strand& strand)
                : session_socket_(sock)
                , session_strand_(strand)
                , request_strand_(session_strand_.context())
            {}

            template<typename Session>
            [[nodiscard]] std::future<std::vector<char>>
                suspend_recv_request(std::promise<std::vector<char>> recv_promise, std::shared_ptr<Session> self)
            {
                auto recv_future = recv_promise.get_future();
                boost::asio::post(request_strand_,
                    [recv_promise = std::move(recv_promise), this, self = std::move(self)]() mutable
                {
                    requests_.push_back(std::move(recv_promise));
                    if (is_streambuf_available()) return reply_recv_request();
                    boost::asio::dispatch(session_strand_,
                        [recv_promise = std::move(recv_promise), this, self = std::move(self)]() mutable
                    {
                        // if (!session_socket_.is_open()) return clear_request();
                        // if (std::exchange(disposing_, true)) return;
                        if (std::exchange(disposing_, true) || !session_socket_.is_open()) return clear_request();
                        dispose_receive(std::move(self));
                    });
                });
                return recv_future;
            }

            template<typename Session>
            void dispose_receive(std::shared_ptr<Session> self)
            {
                assert(disposing_);
                assert(session_strand_.running_in_this_thread());
                boost::asio::defer(session_strand_, [this, self = std::move(self)]() mutable
                {
                    const auto& delim_ref = next_delim();
                    boost::asio::async_read_until(session_socket_, streambuf_, delim_ref,
                        boost::asio::bind_executor(session_strand_, [delim_size = static_cast<uint16_t>(delim_ref.size()),
                            this, self = std::move(self)](boost::system::error_code error, std::size_t transfer_size) mutable
                    {
                        if (delim_size == transfer_size) fmt::print("empty net pack received\n");
                        const auto consume_size = std::atomic_exchange(&recvbuf_consume_size_, 0);
                        if (consume_size > 0) streambuf_.consume(consume_size);
                        fmt::print(">>handle: transferred {} ", transfer_size - consume_size);
                        boost::asio::post(request_strand_,
                            [delim_size, transfer_size = transfer_size - consume_size, this, self]() mutable
                        {
                            if (!has_request()) return;
                            reply_recv_request(delim_size, transfer_size);
                        });
                        if (!error && transfer_size != 0 && delim_size < transfer_size) return dispose_receive(std::move(self));
                        disposing_ = false;
                    }));
                });
            }

            bool finish_receive() const noexcept
            {
                assert(request_strand_.running_in_this_thread());
                return !has_request() && !is_streambuf_available();
            }

            void reply_recv_request(uint16_t delim_size = 0, uint64_t transfer_size = 0)
            {
                assert(request_strand_.running_in_this_thread());
                assert(has_request());
                assert(is_streambuf_available() || delim_size != 0 && transfer_size != 0);
                const auto[vector_size, streambuf_size, has_external_request] = reply_request(delim_size, transfer_size);
                boost::ignore_unused(has_external_request);
                const auto expect_size = transfer_size - delim_size;
                assert(vector_size == expect_size || streambuf_size == expect_size);
                std::atomic_fetch_add(&recvbuf_consume_size_, transfer_size);
            }

            struct external_facet
            {
                boost::asio::streambuf streambuf;
                std::shared_mutex streambuf_mutex;
                std::optional<std::promise<std::unique_lock<std::shared_mutex>>> request;
            };

            static_assert(std::is_default_constructible_v<external_facet>);

            boost::asio::ip::tcp::socket& session_socket_;
            boost::asio::io_context::strand& session_strand_;
            boost::asio::io_context::strand request_strand_;
            std::atomic<uint64_t> recvbuf_consume_size_ = 0;
            boost::asio::streambuf streambuf_;
            std::deque<std::pair<uint16_t, uint64_t>> streambuf_infos_;
            std::deque<std::promise<std::vector<char>>> requests_;
            std::unique_ptr<external_facet> external_;

        private:
            bool is_streambuf_available() const noexcept
            {
                return !streambuf_infos_.empty();
            }

            bool has_request() const noexcept
            {
                return !requests_.empty() || external_ != nullptr && external_->request.has_value();
            }

            std::pair<uint16_t, uint64_t> rotate_streambuf_info(uint16_t delim_size, uint64_t transfer_size)
            {
                if (streambuf_infos_.empty()) return std::make_pair(delim_size, transfer_size);
                streambuf_infos_.emplace_back(delim_size, transfer_size);
                return rotate_streambuf_info();
            }

            std::pair<uint16_t, uint64_t> rotate_streambuf_info()
            {
                assert(!streambuf_infos_.empty());
                const auto front_info = streambuf_infos_.front();
                streambuf_infos_.pop_front();
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
                    if (!requests_.empty())
                    {
                        std::vector<char> streambuf_front(transfer_size - delim_size);
                        const auto copy_size = buffer_copy(boost::asio::buffer(streambuf_front), streambuf_.data());
                        assert(copy_size == streambuf_front.size());
                        std::get<0>(reply_context) = copy_size;
                        requests_.front().set_value(std::move(streambuf_front));
                        requests_.pop_front();
                    }
                    if (external_ != nullptr)
                    {
                        const auto copy_size = boost::asio::buffer_copy(
                            external_->streambuf.prepare(transfer_size - delim_size), streambuf_.data());
                        std::get<1>(reply_context) = copy_size;
                        external_->streambuf.commit(copy_size);
                        if (external_->request.has_value())
                        {
                            external_->request->set_value(std::unique_lock<std::shared_mutex>{external_->streambuf_mutex});
                            std::get<2>(reply_context) = true;
                            external_->request.reset();              //  optional::reset
                        }
                    }
                }
                else clear_request();
                return reply_context;
            }

            void clear_request()
            {
                if (!requests_.empty())
                {
                    for (auto& promise : requests_) promise.set_value(std::vector<char>{});
                    requests_.clear();
                }
                if (external_ != nullptr && external_->request.has_value())
                {
                    external_->request->set_value(std::unique_lock<std::shared_mutex>{external_->streambuf_mutex});
                    external_->request.reset();
                }
            }

        };
    }
}