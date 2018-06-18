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
        inline namespace encoding
        {
            struct use_chunk_t {};
            inline constexpr use_chunk_t use_chunk;
        }
    }

    inline constexpr size_t default_max_chunk_size{ 128_kbyte };
    inline constexpr size_t default_max_chunk_quantity{ 1024 };

    inline void run_io_context(boost::asio::io_context& io_context)
    {
        try
        {
            io_context.run();
        } catch (std::exception const& e)
        {
            core::inspect_exception(e);
        } catch (boost::exception const& e)
        {
            core::inspect_exception(e);
        }
    }

    std::string config_path(core::as_view_t) noexcept;

    std::filesystem::path config_path() noexcept;

    boost::property_tree::ptree const& config();
}