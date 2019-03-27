#pragma once
#include "core/spatial.hpp"
#include "network/net.h"
#include <folly/futures/FutureSplitter.h>

namespace net::protocal
{
    struct dash final : http
    {
        int64_t last_tile_index = 1;

        struct represent final
        {
            int id = 0;
            int bandwidth = 0;
            int qp = 0;
            std::string media;
            std::string initial;
            std::optional<
                folly::FutureSplitter<std::shared_ptr<multi_buffer>>
            > initial_buffer;
        };

        struct adaptation_set
        {
            std::string codecs;
            std::string mime_type;
            boost::container::small_vector<represent, 5> represents;
        };

        struct video_adaptation_set final : adaptation_set, core::coordinate, core::dimension
        {
            struct context;
            std::shared_ptr<context> context;
        };

        struct audio_adaptation_set final : adaptation_set
        {
            int sample_rate = 0;
        };

        class parser final
        {
            struct impl;
            std::shared_ptr<impl> impl_;

        public:
            explicit parser(std::string_view xml_text,
                            std::shared_ptr<folly::ThreadPoolExecutor> executor);
            parser(const parser&) = default;
            parser(parser&&) = default;
            parser& operator=(const parser&) = default;
            parser& operator=(parser&&) = default;
            ~parser() = default;

            std::string_view title() const;
            core::coordinate grid() const;
            core::dimension scale() const;

            video_adaptation_set& video_set(core::coordinate coordinate) const;
            audio_adaptation_set& audio_set() const;

            static std::chrono::milliseconds parse_duration(std::string_view duration);
        };
    };
}
