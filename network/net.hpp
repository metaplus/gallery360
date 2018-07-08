#pragma once

namespace net
{
    namespace protocal
    {
        struct http
        {
            using under_layer_protocal = boost::asio::ip::tcp;
            using socket = boost::asio::ip::tcp::socket;
            static constexpr auto default_version = 11;
            static constexpr auto default_method = boost::beast::http::verb::get;
        };
    }

    inline namespace tag
    {
        inline namespace encoding
        {
            inline struct use_chunk_t {} use_chunk;
        }
    }

    inline constexpr size_t default_max_chunk_size{ 128_kbyte };
    inline constexpr size_t default_max_chunk_quantity{ 1024 };

    std::string config_path(core::as_view_t) noexcept;

    std::filesystem::path config_path() noexcept;

    boost::property_tree::ptree const& config();

    template<typename Entry>
    Entry config_entry(std::string_view entry_name)
    {
        return config().get<Entry>(entry_name.data());
    }

    namespace detail
    {
        class state_base
        {
            enum state_index { active, state_size };

            folly::AtomicBitSet<state_size> state_;
        protected:
            bool is_active() const
            {
                return state_.test(active, std::memory_order_acquire);
            }

            bool is_active(bool active)
            {
                return state_.set(state_index::active, active, std::memory_order_release);
            }
        };
    }
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
