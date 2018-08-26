#pragma once

namespace net
{
	namespace protocal
	{
		struct http
		{
			static constexpr auto default_version = 11;
			static constexpr auto default_method = boost::beast::http::verb::get;

			struct protocal_base
			{
				template<typename Body>
				using response_type = boost::beast::http::response<Body, boost::beast::http::fields>;
				template<typename Body>
				using request_type = boost::beast::http::request<Body, boost::beast::http::fields>;
				template<typename Body>
				using response_parser_type = boost::beast::http::response_parser<Body>;
				template<typename Body>
				using request_parser_type = boost::beast::http::request_parser<Body>;
				template<typename Body>
				using response_ptr = std::unique_ptr<response_type<Body>>;
				template<typename Body>
				using request_ptr = std::unique_ptr<request_type<Body>>;
				template<typename Body>
				using response_promise = std::promise<response_type<Body>>;
				template<typename Body>
				using request_promise = std::promise<request_type<Body>>;
				using under_protocal_type = boost::asio::ip::tcp;
				using socket_type = boost::asio::ip::tcp::socket;
			};
		};

		struct tcp
		{
			struct protocal_base
			{
				using protocal_type = boost::asio::ip::tcp;
				using socket_type = boost::asio::ip::tcp::socket;
				using context_type = boost::asio::io_context;
			};
		};

		struct dash : http
		{
			int64_t last_tile_index = 1;
		};
	}

	inline namespace tag
	{
		inline namespace encoding
		{
			inline struct use_chunk_t {} use_chunk;
		}
	}

	std::vector<boost::thread*> create_asio_threads(boost::asio::io_context& context,
													boost::thread_group& thread_group,
													uint32_t num = std::thread::hardware_concurrency());
	std::vector<std::thread> create_asio_named_threads(boost::asio::io_context& context,
													   uint32_t num = std::thread::hardware_concurrency());

	template<typename... Options>
	struct policy;

	inline constexpr size_t default_max_chunk_size{ 128_kbyte };
	inline constexpr size_t default_max_chunk_quantity{ 1024 };

	template<typename Body>
	boost::beast::http::request<Body> make_http_request(std::string_view host, std::string_view target) {
		static_assert(boost::beast::http::is_body<Body>::value);
		boost::beast::http::request<Body> request;
		request.version(protocal::http::default_version);
		request.method(protocal::http::default_method);
		request.target(target.data());
		request.keep_alive(true);
		request.set(boost::beast::http::field::host, host);
		//request.set(boost::beast::http::field::user_agent, "MetaPlusClient");
		return request;
	}

	std::string config_path(core::as_view_t) noexcept;
	std::filesystem::path config_path() noexcept;
	std::string config_entry(std::string_view entry_name);

	template<typename T>
	T config_entry(std::string_view entry_name) {
		auto entry = config_entry(entry_name);
		if constexpr (meta::is_within<T, std::string, std::string_view>::value) {
			return entry;
		} else {
			return boost::lexical_cast<T>(entry);
		}
	}

	namespace detail
	{
		class state_base
		{
			enum state_index { active, state_size };
			folly::AtomicBitSet<state_size> state_;

		protected:
			bool is_active() const {
				return state_.test(active, std::memory_order_acquire);
			}

			bool is_active(bool active) {
				return state_.set(state_index::active, active, std::memory_order_release);
			}
		};
	}
}

template<typename Protocal>
struct std::less<boost::asio::basic_socket<Protocal>>
{
	bool operator()(boost::asio::basic_socket<Protocal> const& sock1,
					boost::asio::basic_socket<Protocal> const& sock2) const {
		return sock1.remote_endpoint() < sock2.remote_endpoint()
			|| !(sock2.remote_endpoint() < sock1.remote_endpoint())
			&& sock1.local_endpoint() < sock2.local_endpoint();
	}
};

template<typename Protocal>
struct std::equal_to<boost::asio::basic_socket<Protocal>>
{
	bool operator()(boost::asio::basic_socket<Protocal> const& sock1,
					boost::asio::basic_socket<Protocal> const& sock2) const {
		return sock1.remote_endpoint() == sock2.remote_endpoint()
			&& sock1.local_endpoint() == sock2.local_endpoint();
	}
};
