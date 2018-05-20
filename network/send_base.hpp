#pragma once

namespace net
{
    namespace base
    {
        template<typename Socket>
        class send_base;

        template<>
        class send_base<boost::asio::ip::tcp::socket> : context_base
        {
        protected:
            send_base(boost::asio::ip::tcp::socket& sock, boost::asio::io_context::strand& strand)
                : session_socket_(sock)
                , session_strand_(strand)
            {}

            template<typename Session>
            void suspend_sendbuf_handle(boost::asio::const_buffer sendbuf_view, 
                std::unique_ptr<std::any> sendbuf_handle, std::shared_ptr<Session> self)
            {
                boost::asio::post(session_strand_,
                    [sendbuf_view, sendbuf_handle = std::move(sendbuf_handle), this, self = std::move(self)]() mutable
                {
                    sendbuf_handles_.emplace_back(sendbuf_view, std::move(sendbuf_handle));
                    if (std::exchange(disposing_, true) || !session_socket_.is_open()) return;
                    dispose_send(std::move(self));
                });
            }

            template<typename Session>
            void dispose_send(std::shared_ptr<Session> self)
            {
                assert(disposing_);
                assert(session_strand_.running_in_this_thread());
                boost::asio::defer(session_strand_, [this, self = std::move(self)]() mutable
                {
                    if (sendbuf_handles_.empty())
                    {
                        disposing_ = false;
                        return fmt::print("stop dispose send\n");
                        // throw send_finish{ "send finish" };
                    }
                    auto[sendbuf_view, sendbuf_handle] = std::move(sendbuf_handles_.front());   //  deque::iterator is unstable
                    sendbuf_handles_.pop_front();
                    std::array<boost::asio::const_buffer, 2> sendbuf_delim_view{ sendbuf_view, boost::asio::buffer(next_delim()) };
                    boost::asio::async_write(session_socket_, sendbuf_delim_view,
                        boost::asio::bind_executor(session_strand_, [expect_size = buffer_size(sendbuf_delim_view), sendbuf_handle = std::move(sendbuf_handle),
                            this, self = std::move(self)](boost::system::error_code error, std::size_t transfer_size) mutable
                    {
                        //const auto guard = make_fault_guard(error);
                        fmt::print(">>handle: transferred {}, next to transfer {}, queue size {}\n", transfer_size,
                            sendbuf_handles_.empty() ? 0 : sendbuf_handles_.front().first.size(), sendbuf_handles_.size());
                        assert(expect_size == transfer_size);
                        if (sendbuf_handle) sendbuf_handle->reset();                            //  any::reset()
                        if (!error && expect_size == transfer_size) return dispose_send(std::move(self));
                        fmt::print(">>handle: stop dispose send\n");
                        disposing_ = false;
                        throw send_error{ fmt::format("send error num {}, message {}, expect {} / transfer {}\n",
                            error.value(),error.message(),expect_size,transfer_size) };
                        //dispose_close_socket();
                        //send_disposing_ = false;
                    }));
                });
            }

            bool finish_send() const noexcept
            {
                return sendbuf_handles_.empty();
            }

            boost::asio::ip::tcp::socket& session_socket_;
            boost::asio::io_context::strand& session_strand_;
            std::deque<std::pair<boost::asio::const_buffer, std::unique_ptr<std::any>>> sendbuf_handles_;
        };
    }
}