#pragma once

namespace net
{
    using namespace core::literals;

    inline constexpr size_t default_max_chunk_size = 128_kbyte;
    inline constexpr size_t default_max_chunk_quantity = 1024;

    using boost::beast::http::empty_body;
    using boost::beast::http::file_body;
    using boost::beast::http::string_body;
    using boost::beast::http::dynamic_body;
    using boost::beast::http::buffer_body;
    using boost::beast::multi_buffer;
    using boost::beast::flat_buffer;

    namespace protocal
    {
        template <typename Protocal>
        struct protocal_base;

        struct http
        {
            constexpr static auto default_version = 11;
            constexpr static auto default_method = boost::beast::http::verb::get;
        };

        template <>
        struct protocal_base<http>
        {
            template <typename Body>
            using response = boost::beast::http::response<Body, boost::beast::http::fields>;
            template <typename Body>
            using request = boost::beast::http::request<Body, boost::beast::http::fields>;
            template <typename Body>
            using response_parser = boost::beast::http::response_parser<Body>;
            template <typename Body>
            using request_parser = boost::beast::http::request_parser<Body>;
            template <typename Body>
            using response_ptr = std::unique_ptr<response<Body>>;
            template <typename Body>
            using request_ptr = std::unique_ptr<request<Body>>;
            using underlying_protocal = boost::asio::ip::tcp;
            using socket_type = boost::asio::ip::tcp::socket;
        };

        struct tcp final { };

        struct udp final { };

        template <>
        struct protocal_base<tcp>
        {
            using protocal_type = boost::asio::ip::tcp;
            using socket_type = boost::asio::ip::tcp::socket;
            using socket_ptr = std::unique_ptr<boost::asio::ip::tcp::socket>;
            using context_type = boost::asio::io_context;
        };

        template <>
        struct protocal_base<udp>
        {
            using protocal_type = boost::asio::ip::udp;
            using socket_type = boost::asio::ip::udp::socket;
            using socket_ptr = std::unique_ptr<boost::asio::ip::udp::socket>;
            using context_type = boost::asio::io_context;
        };
    }

    template <typename Body>
    boost::beast::http::request<Body> make_http_request(const std::string& host,
                                                        const std::string& target) {
        static_assert(boost::beast::http::is_body<Body>::value);
        assert(!std::empty(host));
        assert(!std::empty(target));
        boost::beast::http::request<Body> request;
        request.version(protocal::http::default_version);
        request.method(protocal::http::default_method);
        request.target(target.data());
        request.keep_alive(true);
        request.set(boost::beast::http::field::host, host);
        return request;
    }

    void add_config_path(std::filesystem::path&& path);
    const std::filesystem::path& config_path(bool json = true) noexcept;
    std::string config_xml_entry(std::vector<std::string> entry_path);
    nlohmann::json::reference config_json_entry(std::vector<std::string> entry_path);

    template <typename T>
    T config_entry(std::string_view entry_name) {
        std::vector<std::string> entry_path;
        folly::split('.', entry_name, entry_path);
        return config_json_entry(entry_path).get<T>();
    }

    boost::container::small_vector<std::thread, 8>
    make_asio_threads(boost::asio::io_context& context,
                      unsigned concurrency = std::thread::hardware_concurrency());

    std::shared_ptr<boost::asio::io_context> make_asio_pool(unsigned concurrency);
}

template <typename Protocal>
struct std::less<boost::asio::basic_socket<Protocal>> final
{
    bool operator()(const boost::asio::basic_socket<Protocal>& sock1,
                    const boost::asio::basic_socket<Protocal>& sock2) const {
        return sock1.remote_endpoint() < sock2.remote_endpoint()
            || !(sock2.remote_endpoint() < sock1.remote_endpoint())
            && sock1.local_endpoint() < sock2.local_endpoint();
    }
};

template <typename Protocal>
struct std::equal_to<boost::asio::basic_socket<Protocal>> final
{
    bool operator()(const boost::asio::basic_socket<Protocal>& sock1,
                    const boost::asio::basic_socket<Protocal>& sock2) const {
        return sock1.remote_endpoint() == sock2.remote_endpoint()
            && sock1.local_endpoint() == sock2.local_endpoint();
    }
};

template <typename Protocal>
struct std::hash<boost::asio::ip::basic_endpoint<Protocal>> final
{
    using argument_type = boost::asio::ip::basic_endpoint<Protocal>;
    using result_type = size_t;

    [[nodiscard]] size_t operator()(const argument_type& endpoint) const {
        size_t seed = 0;
        if (auto&& address = endpoint.address(); address.is_v4()) {
            boost::hash_combine(seed, address.to_v4().to_uint());
        } else if (address.is_v6()) {
            boost::hash_combine(seed, address.to_v6().to_bytes());
        } else {
            boost::hash_combine(seed, address.to_string());
        }
        boost::hash_combine(seed, endpoint.port());
        return seed;
    }
};
