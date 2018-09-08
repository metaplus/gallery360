#pragma once

namespace net
{
    using boost::beast::multi_buffer;
}

namespace net::component
{
    class dash_streamer;

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

        using ordinal = std::pair<int16_t, int16_t>;
        using frame_consumer = folly::Function<void()>;
        using frame_consumer_builder = folly::Function<frame_consumer(std::vector<multi_buffer>)>;
        using buffer_supplier = folly::Function<multi_buffer()>;
        using probability_predictor = folly::Function<double(ordinal)>;

        static boost::future<dash_manager> async_create_parsed(std::string mpd_url);

        std::pair<int16_t, int16_t> scale_size() const;
        std::pair<int16_t, int16_t> grid_size() const;

        folly::Function<multi_buffer()> tile_supplier(
            int row, int column, folly::Function<double()> predictor);

        void register_predictor(probability_predictor predictor);
        void register_represent_consumer(frame_consumer_builder builder) const;

        // exception if tile drained

        void wait_all_tile_consumed();

    private:
        folly::Function<size_t(ordinal)> represent_indexer(folly::Function<double()> probability);
    };
}
