#include "stdafx.h"
#include "command.h"
#include <boost/process/args.hpp>
#include <boost/process/exe.hpp>
#include <boost/process/search_path.hpp>
#include <boost/process/system.hpp>
#include <boost/range/adaptor/indexed.hpp>
#include <tinyxml2.h>

using boost::process::system;
using boost::process::exe;
using boost::process::args;
using boost::adaptors::indexed;

auto ffmpeg_path = [] {
    return boost::process::search_path("ffmpeg");
};

auto mp4box_path = [] {
    return boost::process::search_path("MP4Box");
};

auto ceil_even = [](const int num) constexpr {
    return num % 2 != 0 ? num + 1 : num;
};

bool operator<(media::filter_param l, media::filter_param r) {
    return l.wcrop < r.wcrop || (r.wcrop == l.wcrop && l.hcrop < r.hcrop);
}

namespace media
{
    auto output_directory = []() -> decltype(auto) {
        assert(!std::empty(command::output_directory));
        assert(is_directory(command::output_directory));
        return (command::output_directory);
    };

    auto output_file_directory = [](const std::string_view input) {
        const auto file_stem = std::filesystem::path{ input }.stem();
        return output_directory() / file_stem;
    };

    auto output_h264_directory = [](const std::string_view input, const std::string_view param) {
        const auto file_stem = std::filesystem::path{ input }.stem();
        return std::make_pair(file_stem.generic_string(),
                              output_directory() / file_stem / param / "h264");
    };

    auto output_mp4_directory = [](const std::string_view input, const std::string_view param) {
        const auto file_stem = std::filesystem::path{ input }.stem();
        return std::make_pair(file_stem.generic_string(),
                              output_directory() / file_stem / param / "mp4");
    };

    auto output_path = [](const std::string_view input) {
        const auto filename = std::filesystem::path{ input }.filename();
        return output_directory() / filename;
    };

    auto output_path_string = [](const std::string_view input) {
        return output_path(input).generic_string();
    };

    auto mesh_description = [](const filter_param filter, const rate_control rate) {
        return fmt::format("{}x{}_{}", filter.wcrop, filter.hcrop, rate.bit_rate);
    };

    auto mesh_regex = [](media::filter_param filter)-> decltype(auto) {
        static std::map<std::pair<int, int>, std::regex> cache_mesh_regex;
        auto cache_key = std::make_pair(filter.wcrop, filter.hcrop);
        if (const auto regex_iter = cache_mesh_regex.find(cache_key); regex_iter != cache_mesh_regex.end()) {
            return (regex_iter->second);
        }
        const auto regex_content = fmt::format(R"({}x{}_(\d+))", filter.wcrop, filter.hcrop);
        return (cache_mesh_regex.emplace(cache_key, std::regex{ regex_content }).first->second);
    };

    auto mesh_directories = [](media::filter_param filter) {
        return core::filter_directory_entry(
            output_directory(),
            [filter](const std::filesystem::directory_entry& mesh_dir) {
                const auto mesh_desc = mesh_dir.path().stem().generic_string();
                return std::regex_match(mesh_desc, mesh_regex(filter));
            });
    };

    auto tile_mpd_coordinate = [](std::string_view filename) {
        static const std::regex mpd_regex{ R"(\w+_c(\d+)r(\d+)_\d+kbps.mpd)" };
        std::cmatch coordinate;
        if (std::regex_match(filename.data(), coordinate, mpd_regex)) {
            return std::make_pair(
                boost::lexical_cast<int>(coordinate[1]),
                boost::lexical_cast<int>(coordinate[2]));
        }
        core::throw_unreachable(__FUNCTION__);
    };

    auto tile_mpd_path_map = [](filter_param filter) {
        std::map<
            std::pair<int, int>,
            std::vector<std::filesystem::path>
        > mpd_path_map;
        for (auto& mesh_path : mesh_directories(filter)) {
            const auto mpd_paths = core::filter_directory_entry(
                mesh_path / "dash",
                [](const std::filesystem::directory_entry& mpd_entry) {
                    return mpd_entry.path().extension() == ".mpd";
                });
            assert(!std::empty(mpd_paths));
            for (auto& mpd_path : mpd_paths) {
                const auto mpd_coordinate = tile_mpd_coordinate(mpd_path.filename().generic_string());
                mpd_path_map[mpd_coordinate].push_back(mpd_path);
            }
        }
        return mpd_path_map;
    };

    void command::resize(const std::string_view input,
                         const size_param size) {
        system(
            exe = ffmpeg_path(),
            args = {
                "-i", input.data(),
                "-s", fmt::format("{}x{}", size.width, size.height),
                output_path_string(input)
            }
        );
    }

    auto x264_encode_param = [](const rate_control rate) {
        const auto bit_rate = rate.bit_rate / (rate.frame_rate / 30);
        return folly::join(
            ':', {
                fmt::format("keyint={}", 30),
                fmt::format("min-keyint={}", 30),
                fmt::format("bitrate={}", bit_rate),
                fmt::format("vbv-maxrate={}", bit_rate * 2),
                fmt::format("vbv-bufsize={}", bit_rate * 4),
                fmt::format("fps={}", 30),
                "scenecut=0"s,
                "no-scenecut"s,
                "pass=1"s,
            });
    };

    void command::crop_scale_transcode(const std::string_view input,
                                       const filter_param filter,
                                       const rate_control rate,
                                       const pace_control pace) {
        std::vector<std::filesystem::path> output_mesh_trace;
        if (pace.offset < filter.wcrop * filter.hcrop) {
            std::vector<std::string> cmd_params{ "ffmpeg", "-i", input.data() };
            std::string crop_scale_map;
            {
                const auto [width, height] = media::format_context{ media::source::path{ input.data() } }.demux(media::type::video).scale();
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
                        crop_scale_map += fmt::format("[0:v]{},{}[v{}:{}];", crop, scale, i, j);
                    }
                }
                crop_scale_map.pop_back();
            }
            cmd_params.emplace_back(fmt::format("-filter_complex \"{}\"", crop_scale_map));
            const auto [file_stem, file_output_dir] = output_h264_directory(input, mesh_description(filter, rate));
            const auto [remove_count, make_success] = core::make_empty_directory(file_output_dir);
            assert(make_success);
            const auto crop_tile_path = [&file_stem, &file_output_dir, &rate](int i, int j) {
                return (file_output_dir / fmt::format("{}_c{}r{}_{}kbps.264", file_stem, i, j, rate.bit_rate)).generic_string();
            };
            for (auto i = 0; i != filter.wcrop; ++i) {
                for (auto j = 0; j != filter.hcrop; ++j) {
                    if (i * filter.hcrop + j < pace.offset || i * filter.hcrop + j >= pace.offset + pace.stride) {
                        continue;
                    }
                    cmd_params.emplace_back(fmt::format("-map [v{}:{}]", i, j));
                    cmd_params.emplace_back("-c:v libx264");
                    cmd_params.emplace_back("-preset slow");
                    cmd_params.emplace_back("-x264-params");
                    cmd_params.emplace_back(x264_encode_param(rate));
                    cmd_params.emplace_back("-f h264");
                    cmd_params.emplace_back(crop_tile_path(i, j));
                }
            }
            cmd_params.emplace_back("-y");
            system(folly::join(' ', cmd_params));
            crop_scale_transcode(input, filter, rate, { pace.stride, pace.offset + pace.stride });
        }
    }

    void command::package_container(rate_control rate) {
        for (auto& mesh_entry : std::filesystem::directory_iterator{ media::output_directory() }) {
            assert(mesh_entry.is_directory());
            const auto h264_directory = mesh_entry.path() / "h264";
            const auto mp4_directory = mesh_entry.path() / "mp4";
            assert(is_directory(h264_directory));
            const auto [remove_count, make_success] = core::make_empty_directory(mp4_directory);
            assert(make_success);
            for (auto& h264_entry : std::filesystem::directory_iterator{ h264_directory }) {
                auto h264_path = h264_entry.path();
                assert(h264_path.extension() == ".264");
                auto mp4_path = (mp4_directory / h264_path.filename()).replace_extension(".mp4");
                system(
                    exe = mp4box_path(),
                    args = {
                        "-add", h264_path.generic_string(),
                        "-fps", fmt::to_string(rate.frame_rate),
                        mp4_path.generic_string()
                    }
                );
            }
        }
    }

    void command::dash_segment(const std::chrono::milliseconds duration) {
        for (auto& mesh_entry : std::filesystem::directory_iterator{ media::output_directory() }) {
            assert(mesh_entry.is_directory());
            const auto mp4_directory = mesh_entry.path() / "mp4";
            const auto dash_directory = mesh_entry.path() / "dash";
            assert(is_directory(mp4_directory));
            const auto [remove_count, make_success] = core::make_empty_directory(dash_directory);
            assert(make_success);
            for (auto& mp4_entry : std::filesystem::directory_iterator{ mp4_directory }) {
                auto mp4_path = mp4_entry.path();
                assert(std::filesystem::is_regular_file(mp4_path));
                assert(mp4_path.extension() == ".mp4");
                auto dash_path = (dash_directory / mp4_path.filename()).replace_extension(".mpd");
                std::vector<std::string> cmd_params{ "mp4box" };
                cmd_params.emplace_back(fmt::format("-dash {}", duration.count()));
                cmd_params.emplace_back(fmt::format("-frag {}", duration.count()));
                cmd_params.emplace_back("-profile live");
                cmd_params.emplace_back("-rap");
                cmd_params.emplace_back(fmt::format("-out {}", dash_path.generic_string()));
                cmd_params.emplace_back(mp4_path.generic_string());
                system(folly::join(' ', cmd_params));
            }
        }
    }

    using tinyxml2::XMLDocument;
    using tinyxml2::XMLElement;
    using tinyxml2::XMLAttribute;

    void command::merge_dash_mpd(const filter_param filter) {
        const auto& mpd_directory = output_directory;
        auto xml_check = folly::lazy([] {
            return (core::check[tinyxml2::XML_SUCCESS]);
        });
        for (auto& [coordinate, mpd_paths] : tile_mpd_path_map(filter)) {
            for (auto& mpd_path : mpd_paths) {
                XMLDocument mpd_document;
                xml_check() << mpd_document.LoadFile(mpd_path.generic_string().data());
                auto str = mpd_document.RootElement()->GetText();
            }
        }
    }
}
