#pragma once

namespace net::component
{
    class dash_streamer;

    namespace detail
    {
        using boost::asio::const_buffer;
        using boost::beast::multi_buffer;
    }

    class dash_manager final
    {
        struct impl;
        std::shared_ptr<impl> impl_;

        explicit dash_manager(std::string&& mpd_url, unsigned concurrency = 2);

    public:
        dash_manager() = delete;
        dash_manager(dash_manager const&) = default;
        dash_manager(dash_manager&&) = default;
        dash_manager& operator=(dash_manager const&) = default;
        dash_manager& operator=(dash_manager&&) = default;
        ~dash_manager() = default;

        using ordinal = std::pair<int, int>;
        using frame_consumer = folly::Function<bool()>;
        using frame_consumer_builder = folly::Function<frame_consumer(std::list<detail::const_buffer>)>;

        //static boost::future<dash_manager> async_create_parsed(std::string mpd_url);
        static folly::Future<dash_manager> async_create_parsed(std::string mpd_url);

        std::pair<int, int> scale_size() const;
        std::pair<int, int> grid_size() const;

        folly::Function<detail::multi_buffer()> tile_supplier(
            int row, int column, folly::Function<double()> predictor);

        void register_represent_builder(frame_consumer_builder builder) const;

        // exception if tile drained
        void wait_full_frame_consumed();

    private:
        folly::Function<size_t(int, int)> represent_indexer(folly::Function<double(int, int)> probability);
    };
}
