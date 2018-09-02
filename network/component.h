#pragma once

namespace net::component
{
    class dash_streamer;

    class dash_manager final
    {
        struct impl;
        std::shared_ptr<impl> impl_;

        explicit dash_manager(std::string&& mpd_url);

    public:
        dash_manager() = delete;
        dash_manager(dash_manager const&) = default;
        dash_manager(dash_manager&&) = default;
        dash_manager& operator=(dash_manager const&) = default;
        dash_manager& operator=(dash_manager&&) = default;
        ~dash_manager() = default;

        static boost::future<dash_manager> async_create_parsed(std::string mpd_url);

        std::pair<int16_t, int16_t> spatial_size();
        std::pair<int16_t, int16_t> grid_size();

        folly::Function<boost::asio::const_buffer()> on_supply_tile(std::string tile_url);
    };
}