#include "stdafx.h"
#include "command.h"
#include <boost/process/args.hpp>
#include <boost/process/exe.hpp>
#include <boost/process/search_path.hpp>
#include <boost/process/system.hpp>
#include <boost/range/adaptor/indexed.hpp>

using boost::process::system;
using boost::process::exe;
using boost::process::args;
using boost::adaptors::indexed;

auto ffmpeg_path = [] {
    return boost::process::search_path("ffmpeg");
};

auto ceil_even = [](int num) constexpr {
    return num % 2 != 0 ? num + 1 : num;
};

auto clean_create_directory = [](std::filesystem::path directory) {
    assert(std::filesystem::is_directory(directory.root_directory()));
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
};

namespace media
{
    auto output_directory = []() -> decltype(auto) {
        assert(!std::empty(command::output_directory));
        return (command::output_directory);
    };

    auto output_path = [](std::string_view input) {
        const auto filename = std::filesystem::path{ input }.filename();
        return output_directory() / filename;
    };

    auto output_path_string = [](std::string_view input) {
        return output_path(input).generic_string();
    };

    void command::resize(std::string_view input,
                         size_param size) {
        system(
            exe = ffmpeg_path(),
            args = {
                "-i",input.data(),
                "-s",fmt::format("{}x{}",size.width,size.height),
                output_path_string(input)
            }
        );
    }

    auto x264_encode_param = [](rate_control rate) {
        return folly::join(
            ':', {
                fmt::format("keyint={}",rate.frame_rate),
                fmt::format("min-keyint={}",rate.frame_rate),
                fmt::format("bitrate={}",rate.bit_rate),
                fmt::format("vbv-maxrate={}",rate.bit_rate * 2),
                fmt::format("vbv-bufsize={}",rate.bit_rate * 4),
                fmt::format("fps={}",rate.frame_rate),
                "scenecut=0"s,
                "no-scenecut"s,
                "pass=1"s,
            });
    };

    void command::crop_scale_transcode(const std::string_view input,
                                       filter_param filter,
                                       rate_control rate,
                                       pace_control pace) {
        if (pace.offset < filter.wcrop * filter.hcrop) {
            std::vector<std::string> params{ "ffmpeg","-i",input.data() };
            std::string crop_scale_map;
            const auto[width, height] = media::format_context{ media::source::path{ input.data() } }.demux(media::type::video).scale();
            const auto scale = fmt::format("scale={}:{}",
                                           ceil_even(width / filter.wcrop / filter.wscale),
                                           ceil_even(height / filter.hcrop / filter.hscale));
            for (auto i = 0; i != filter.wcrop; ++i) {
                for (auto j = 0; j != filter.hcrop; ++j) {
                    if (i * filter.hcrop + j < pace.offset || i * filter.hcrop + j >= pace.offset + pace.stride) {
                        continue;
                    }
                    const auto crop = fmt::format("crop={}:{}:{}:{}",
                                                  width / filter.wcrop, height / filter.hcrop,
                                                  width * i / filter.wcrop, height * j / filter.hcrop);
                    crop_scale_map.append(fmt::format("[0:v]{},{}[v{}:{}];", crop, scale, i, j));
                }
            }
            crop_scale_map.pop_back();
            params.push_back(fmt::format("-filter_complex \"{}\"", crop_scale_map));
            const auto file_stem = std::filesystem::path{ std::string{ input } }.stem().generic_string();
            const auto file_output_dir = output_directory / file_stem / fmt::format("{}x{}_{}", filter.wcrop, filter.hcrop, rate.bit_rate);
            clean_create_directory(file_output_dir);
            const auto crop_tile_path = [&file_stem, &file_output_dir, &rate](int i, int j) {
                return (file_output_dir / fmt::format("{}_{}_{}_{}.mp4", file_stem, i, j, rate.bit_rate)).generic_string();
            };
            for (auto i = 0; i != filter.wcrop; ++i) {
                for (auto j = 0; j != filter.hcrop; ++j) {
                    if (i * filter.hcrop + j < pace.offset || i * filter.hcrop + j >= pace.offset + pace.stride) {
                        continue;
                    }
                    params.push_back(fmt::format("-map [v{}:{}]", i, j));
                    params.push_back("-c:v libx264");
                    params.push_back("-preset slow");
                    params.push_back("-x264-params");
                    params.push_back(x264_encode_param(rate));
                    params.push_back(crop_tile_path(i, j));
                }
            }
            params.push_back("-y");
            system(folly::join(' ', params));
            crop_scale_transcode(input, filter, rate, { pace.stride, pace.offset + pace.stride });
        }
    }
}