#pragma once

namespace net
{
    namespace detail
    {
        using boost::asio::const_buffer;
        using boost::beast::multi_buffer;
        using trace_callback = std::function<void(std::string_view, std::string)>;
        using predict_callback = std::function<double(int, int)>;
    }

    struct buffer_sequence final
    {
        detail::multi_buffer& initial;
        detail::multi_buffer data;

        buffer_sequence(detail::multi_buffer& initial, detail::multi_buffer&& data);
        buffer_sequence(buffer_sequence&) = delete;
        buffer_sequence(buffer_sequence&& that) noexcept;
        buffer_sequence& operator=(buffer_sequence&) = delete;
        buffer_sequence& operator=(buffer_sequence&&) = delete;
        ~buffer_sequence() = default;
    };
}

namespace net::component
{
    using ordinal = std::pair<int, int>;

    class dash_manager final
    {
        struct impl;
        std::shared_ptr<impl> impl_;

        dash_manager(std::string&& mpd_url, unsigned concurrency,
                     std::shared_ptr<folly::ThreadPoolExecutor> executor);

    public:
        dash_manager() = delete;
        dash_manager(const dash_manager&) = default;
        dash_manager(dash_manager&&) noexcept = default;
        dash_manager& operator=(const dash_manager&) = default;
        dash_manager& operator=(dash_manager&&) noexcept = default;
        ~dash_manager() = default;

        static folly::Future<dash_manager> create_parsed(
            std::string mpd_url, unsigned concurrency = std::thread::hardware_concurrency(),
            std::shared_ptr<folly::ThreadPoolExecutor> executor = nullptr);

        core::dimension frame_size() const;
        core::dimension tile_size() const;
        core::coordinate grid_size() const;
        int tile_count() const;
        void trace_by(detail::trace_callback callback) const;
        void predict_by(detail::predict_callback callback) const;
        bool available() const;
        folly::Function<folly::SemiFuture<buffer_sequence>()> tile_buffer_streamer(core::coordinate coordinate);
    };
}
