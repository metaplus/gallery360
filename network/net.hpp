#pragma once

namespace net
{
    namespace protocal
    {
        struct http
        {
            using lower_protocal = boost::asio::ip::tcp;
            using socket = boost::asio::ip::tcp::socket;
        };
    }

    inline namespace tag
    {
        inline namespace encode
        {
            struct use_chunk_t {};
            inline constexpr use_chunk_t use_chunk;
        }
    }

    struct send_finish : std::logic_error
    {
        using std::logic_error::logic_error;
        using std::logic_error::operator=;
    };

    struct send_error : std::runtime_error
    {
        using std::runtime_error::runtime_error;
        using std::runtime_error::operator=;
    };

    struct recv_finish : std::logic_error
    {
        using std::logic_error::logic_error;
        using std::logic_error::operator=;
    };

    struct recv_error : std::runtime_error
    {
        using std::runtime_error::runtime_error;
        using std::runtime_error::operator=;
    };
}