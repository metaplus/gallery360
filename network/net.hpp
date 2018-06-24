#pragma once

namespace net
{
    namespace protocal
    {
        struct http
        {
            using under_layer_protocal = boost::asio::ip::tcp;
            using socket = boost::asio::ip::tcp::socket;
        };
    }

    inline namespace tag
    {
        inline namespace encoding
        {
            struct use_chunk_t {};
            inline constexpr use_chunk_t use_chunk;
        }
    }

    inline constexpr size_t default_max_chunk_size{ 128_kbyte };
    inline constexpr size_t default_max_chunk_quantity{ 1024 };

    std::string config_path(core::as_view_t) noexcept;

    std::filesystem::path config_path() noexcept;

    boost::property_tree::ptree const& config();
}

template<typename Protocal>
struct std::less<boost::asio::basic_socket<Protocal>>
{
    bool operator()(boost::asio::basic_socket<Protocal> const& sock1, boost::asio::basic_socket<Protocal> const& sock2) const
    {
        return sock1.remote_endpoint() < sock2.remote_endpoint()
            || !(sock2.remote_endpoint() < sock1.remote_endpoint())
            && sock1.local_endpoint() < sock2.local_endpoint();
    }
};

template<typename Protocal>
struct std::equal_to<boost::asio::basic_socket<Protocal>>
{
    bool operator()(boost::asio::basic_socket<Protocal> const& sock1, boost::asio::basic_socket<Protocal> const& sock2) const
    {
        return sock1.remote_endpoint() == sock2.remote_endpoint()
            && sock1.local_endpoint() == sock2.local_endpoint();
    }
};