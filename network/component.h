#pragma once

namespace net::component
{
    class dash_streamer;

    namespace detail
    {
        using boost::asio::const_buffer;
        using boost::beast::multi_buffer;
    }

    using ordinal = std::pair<int, int>;
    using frame_consumer = folly::Function<bool()>;
    using frame_builder = folly::Function<frame_consumer(detail::multi_buffer&,
                                                         detail::multi_buffer&&)>;

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

        //static boost::future<dash_manager> async_create_parsed(std::string mpd_url);
        static folly::Future<dash_manager> async_create_parsed(std::string mpd_url);

        std::pair<int, int> scale_size() const;
        std::pair<int, int> grid_size() const;

        void register_represent_builder(frame_builder builder) const;
        void register_predictor(folly::Function<double(int, int)> predictor);

        bool available() const;

        bool poll_tile_consumed(int col, int row) const;
        bool wait_tile_consumed(int col, int row) const;
        int wait_full_frame_consumed();

    private:
        folly::Function<size_t(int, int)> represent_indexer(folly::Function<double(int, int)> probability);
    };
}
