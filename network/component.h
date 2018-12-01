#pragma once

namespace net
{
    namespace detail
    {
        using boost::asio::const_buffer;
        using boost::beast::multi_buffer;
    }

    struct buffer_context
    {
        detail::multi_buffer& initial;
        detail::multi_buffer data;

        buffer_context(detail::multi_buffer& initial, detail::multi_buffer&& data);
        buffer_context(buffer_context&) = delete;
        buffer_context(buffer_context&& that) noexcept;
        buffer_context& operator=(buffer_context&) = delete;
        buffer_context& operator=(buffer_context&&) = delete;
        ~buffer_context() = default;
    };
}

namespace net::component
{
    using ordinal = std::pair<int, int>;
    using frame_consumer = folly::Function<bool()>;
    using frame_indexed_builder = folly::Function<frame_consumer(std::pair<int, int>,
                                                                 detail::multi_buffer&,
                                                                 detail::multi_buffer&&)>;

    class dash_manager final
    {
        struct impl;
        std::shared_ptr<impl> impl_;

        dash_manager(std::string&& mpd_url, unsigned concurrency);

    public:
        dash_manager() = delete;
        dash_manager(const dash_manager&) = default;
        dash_manager(dash_manager&&) noexcept = default;
        dash_manager& operator=(const dash_manager&) = default;
        dash_manager& operator=(dash_manager&&) noexcept = default;
        ~dash_manager() = default;

        static folly::Future<dash_manager> create_parsed(std::string mpd_url,
                                                         unsigned concurrency = std::thread::hardware_concurrency());

        std::pair<int, int> scale_size() const;
        std::pair<int, int> grid_size() const;

        void predict_by(folly::Function<double(int, int)> predictor);

        bool available() const;

        folly::SemiFuture<buffer_context> request_tile_context(int col, int row) const;

    private:
        folly::Function<size_t(int, int)> represent_indexer(folly::Function<double(int, int)> probability);
    };
}
