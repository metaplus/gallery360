#pragma once

namespace net
{
    inline namespace protocal
    {
        struct http
        {
            using lower_protocal = boost::asio::ip::tcp;
            using socket = boost::asio::ip::tcp::socket;
        };
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
}