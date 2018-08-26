#include "stdafx.h"
#include "acceptor.hpp"

namespace net::server
{
	acceptor<boost::asio::ip::tcp>::acceptor(boost::asio::ip::tcp::endpoint endpoint, boost::asio::io_context& context, bool reuse_addr)
		: context_(context)
		, acceptor_(context, endpoint, reuse_addr)
	{
		core::verify(acceptor_.is_open());
		fmt::print("acceptor: listen address {}, port {}\n", endpoint.address(), listen_port());
	}

	acceptor<boost::asio::ip::tcp>::acceptor(uint16_t port, boost::asio::io_context& context, bool reuse_addr)
		: acceptor(boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4(), port }, context, reuse_addr) {}

	uint16_t acceptor<boost::asio::ip::tcp>::listen_port() const
	{
		return acceptor_.local_endpoint().port();
	}

	boost::future<protocal::tcp::protocal_base::socket_type> acceptor<boost::asio::ip::tcp>::async_listen_socket(pending&& pending)
	{
		auto future_socket = pending.get_future();
		{
			auto wlock = socket_pendlist_.wlock();
			wlock->emplace_back(std::move(pending));
			if (wlock->size() <= 1)
			{
				auto const inactive = is_active(true);
				assert(!inactive);
				boost::asio::post(context_, on_listen_session());
			}
		}
		return future_socket;
	}

	folly::Function<void() const> acceptor<boost::asio::ip::tcp>::on_listen_session()
	{
		return [this]
		{
			acceptor_.async_accept(on_accept());
		};
	}

	folly::Function<void(boost::system::error_code errc, boost::asio::ip::tcp::socket socket)> acceptor<boost::asio::ip::tcp>::on_accept()
	{
		return [this](boost::system::error_code errc, boost::asio::ip::tcp::socket socket)
		{
			fmt::print(std::cout, "acceptor: handle accept errc {}, errmsg {}\n", errc, errc.message());
			auto wlock = socket_pendlist_.wlock();
			if (errc)
			{
				for (auto& socket_pending : *wlock)
					socket_pending.set_exception(std::make_exception_ptr(std::runtime_error{ "acceptor error" }));
				wlock->clear();
				return close_acceptor(errc);
			}
			if (wlock->size() > 1)
				boost::asio::post(context_, on_listen_session());
			else
			{
				auto const active = is_active(false);
				assert(active);
			}
			fmt::print(std::cout, "acceptor: on_accept, local {}, remote {}\n", socket.local_endpoint(), socket.remote_endpoint());
			wlock->front().set_value(std::move(socket));
			wlock->pop_front();
		};
	}

	void acceptor<boost::asio::ip::tcp>::close_acceptor(boost::system::error_code errc)
	{
		fmt::print(std::cerr, "acceptor: close errc {}, errmsg {}\n", errc, errc.message());
		acceptor_.cancel();
		acceptor_.close();
	}
}
