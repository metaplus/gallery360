#pragma once
#include "core/spatial.hpp"
#include <folly/futures/Future.h>
#include <folly/executors/ThreadPoolExecutor.h>
#include <boost/beast/core/multi_buffer.hpp>
#include <spdlog/common.h>

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

namespace net
{
    class dash_manager final
    {
        struct impl;
        std::shared_ptr<impl> impl_;

    public:
        explicit dash_manager(std::string mpd_url, unsigned concurrency = std::thread::hardware_concurrency(),
                              std::shared_ptr<folly::ThreadPoolExecutor> executor = nullptr);
        dash_manager() = delete;
        dash_manager(const dash_manager&) = default;
        dash_manager(dash_manager&&) noexcept = default;
        dash_manager& operator=(const dash_manager&) = default;
        dash_manager& operator=(dash_manager&&) noexcept = default;
        ~dash_manager() = default;

        folly::Future<dash_manager> request_stream_index() const;
        folly::Function<folly::SemiFuture<buffer_sequence>()> tile_streamer(core::coordinate coordinate);
        core::dimension frame_size() const;
        core::dimension tile_size() const;
        core::coordinate grid_size() const;
        int tile_count() const;
        void trace_by(spdlog::sink_ptr sink) const;
        void predict_by(detail::predict_callback callback) const;
        bool available() const;
    };
}
