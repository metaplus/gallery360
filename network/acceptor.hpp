#pragma once

namespace net::server
{
    template<typename Protocal>
    class acceptor;

    template<>
    class acceptor<boost::asio::ip::tcp> : public executor_guard
    {
        boost::asio::ip::tcp::acceptor acceptor_;
        std::list<boost::promise<boost::asio::ip::tcp::socket>> socket_pendlist_;
        boost::asio::io_context::strand pendlist_strand_;
        std::atomic<bool> mutable active_{ false };

    public:
        acceptor(boost::asio::ip::tcp::endpoint endpoint, executor_guard guard, bool reuse_addr = false)
            : executor_guard(guard)
            , acceptor_(context(), endpoint, reuse_addr)
            , pendlist_strand_(context())
        {
            core::verify(acceptor_.is_open());
            fmt::print("acceptor: listen address {}, port {}\n", endpoint.address(), listen_port());
        }

        uint16_t listen_port() const
        {
            return acceptor_.local_endpoint().port();
        }

        // acceptor<boost::asio::ip::tcp>& async_run()
        // {
        //     if (!std::atomic_exchange(&active_, true))
        //         boost::asio::post(acceptor_.get_executor(), [this] { exec_accept(); });
        //     return *this;
        // }

        template<typename ApplicationProtocal, typename ...SessionArgs>
        std::unique_ptr<session<ApplicationProtocal, boost::asio::ip::tcp::socket>>
            listen_session(SessionArgs&& ...args)
        {
            // async_run();
            boost::promise<boost::asio::ip::tcp::socket> socket_promise;
            auto socket_future = socket_promise.get_future();
            boost::asio::dispatch(pendlist_strand_, [this, socket_promise = std::move(socket_promise)]() mutable
            {
                socket_pendlist_.push_back(std::move(socket_promise));
                if (socket_pendlist_.size() > 1) return;
                std::atomic_store(&active_, true);
                boost::asio::post(executor(), [this] { exec_accept(); });
            });
            auto socket = socket_future.get();
            return std::make_unique<session<ApplicationProtocal, boost::asio::ip::tcp::socket>>(
                std::move(socket), static_cast<executor_guard&>(*this), std::forward<SessionArgs>(args)...);
        }

    private:
        void exec_accept()
        {
            acceptor_.async_accept(
                boost::asio::bind_executor(
                    pendlist_strand_, [this](boost::system::error_code errc, boost::asio::ip::tcp::socket socket)
                    {
                        fmt::print(std::cout, "acceptor: errc {}, errmsg {}\n", errc, errc.message());
                        if (errc)
                        {
                            for (auto& socket_pending : socket_pendlist_)
                                socket_pending.set_exception(std::make_exception_ptr(std::runtime_error{ "acceptor error" }));
                            return close_acceptor(errc);
                        }
                        if (socket_pendlist_.size() > 1)
                            boost::asio::post(executor(), [this] { exec_accept(); });
                        fmt::print(std::cout, "acceptor: local{}, remote {}\n", socket.local_endpoint(), socket.remote_endpoint());
                        socket_pendlist_.front().set_value(std::move(socket));
                        socket_pendlist_.pop_front();
                        if (socket_pendlist_.empty())
                            std::atomic_store(&active_, false);
                    }));
        }

        void close_acceptor(boost::system::error_code errc)
        {
            fmt::print(std::cerr, "acceptor: accept errc {}, errmsg {}\n", errc, errc.message());
            // acceptor_.cancel();
            acceptor_.close();
            auto const active_old = std::atomic_exchange(&active_, false);
            assert(active_old);
        }
    };

    template class acceptor<boost::asio::ip::tcp>;
}