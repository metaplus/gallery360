#include "stdafx.h"
#include "command.h"
#include <boost/process/args.hpp>
#include <boost/process/exe.hpp>
#include <boost/process/search_path.hpp>
#include <boost/process/system.hpp>
#include <boost/range/adaptor/indexed.hpp>
#include <tinyxml2.h>
#include <re2/re2.h>

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

    static_assert(!std::is_copy_constructible<RE2>::value);
    static_assert(!std::is_move_constructible<RE2>::value);

    auto mesh_regex = [](media::filter_param filter)-> decltype(auto) {
        static std::map<std::pair<int, int>, RE2> cache_mesh_regex;
        auto cache_key = std::make_pair(filter.wcrop, filter.hcrop);
        if (const auto regex_iter = cache_mesh_regex.find(cache_key); regex_iter != cache_mesh_regex.end()) {
            return (regex_iter->second);
        }
        const auto regex_content = fmt::format(R"({}x{}_(\d+))", filter.wcrop, filter.hcrop);
        return (cache_mesh_regex.try_emplace(cache_key, regex_content)
                                .first->second);
    };

    auto mesh_directories = [](media::filter_param filter) {
        return core::filter_directory_entry(
            output_directory(),
            [filter](const std::filesystem::directory_entry& mesh_dir) {
                const auto mesh_desc = mesh_dir.path().stem().generic_string();
                return RE2::FullMatch(mesh_desc, mesh_regex(filter));
            });
    };

    // Todo: test
    auto tile_mpd_coordinate = [](std::string_view filename) {
        static const RE2 mpd_regex{ R"(\w+_c(\d+)r(\d+)_\d+kbps.mpd)" };
        auto col = 0, row = 0;
        if (RE2::FullMatch(filename.data(), mpd_regex, &col, &row)) {
            return std::make_pair(col, row);
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
                "no-scenecut=1"s,
                "pass=1"s,
            });
    };

    void command::crop_scale_transcode(const std::filesystem::path input,
                                       const rate_control rate,
                                       const pace_control pace) {
        if (pace.offset < wcrop * hcrop) {
            std::vector<std::string> cmd_params{ "ffmpeg", "-i", input.string() };
            std::string crop_scale_map;
            {
                const auto [width, height] = media::format_context{
                    media::source::path{ input.string().data() }
                }.demux(media::type::video).scale();
                const auto scale = fmt::format("scale={}:{}",
                                               ceil_even(width / wcrop / wscale),
                                               ceil_even(height / hcrop / hscale));
                for (auto i = 0; i != wcrop; ++i) {
                    for (auto j = 0; j != hcrop; ++j) {
                        if (i * hcrop + j < pace.offset || i * hcrop + j >= pace.offset + pace.stride) {
                            continue;
                        }
                        const auto crop = fmt::format("crop={}:{}:{}:{}",
                                                      width / wcrop, height / hcrop,
                                                      width * i / wcrop, height * j / hcrop);
                        crop_scale_map += fmt::format("[0:v]{},{}[v{}:{}];", crop, scale, i, j);
                    }
                }
                crop_scale_map.pop_back();
            }
            cmd_params.emplace_back(fmt::format("-filter_complex \"{}\"", crop_scale_map));
            const auto [file_stem, file_output_dir] = output_h264_directory(input.string(),
                                                                            mesh_description({ wcrop, hcrop }, rate));
            {
                auto make_success = false;
                if (pace.offset) {
                    make_success = create_directories(file_output_dir);
                } else {
                    std::tie(std::ignore, make_success) = core::make_empty_directory(file_output_dir);
                    assert(make_success);
                }
            }
            const auto crop_tile_path = [&file_stem, &file_output_dir, &rate](int i, int j) {
                return (file_output_dir / fmt::format("{}_c{}r{}_{}kbps.264", file_stem, i, j, rate.bit_rate)).generic_string();
            };
            for (auto i = 0; i != wcrop; ++i) {
                for (auto j = 0; j != hcrop; ++j) {
                    if (i * hcrop + j < pace.offset || i * hcrop + j >= pace.offset + pace.stride) {
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
            crop_scale_transcode(input, rate, { pace.stride, pace.offset + pace.stride });
        }
    }

    auto mesh_directory = [](const filter_param filter) {
        return core::filter_directory_entry(
            media::output_directory(),
            [filter](const std::filesystem::directory_entry& entry) {
                const auto dirname = entry.path().filename().string();
                const auto regex = fmt::format("{}x{}_\\d+", filter.wcrop, filter.hcrop);
                return RE2::FullMatch(dirname, regex);
            });
    };

    void command::package_container(const rate_control rate) {
        for (auto& mesh_entry : mesh_directory({ wcrop, hcrop })) {
            assert(is_directory(mesh_entry));
            const auto h264_directory = mesh_entry / "h264";
            const auto mp4_directory = mesh_entry / "mp4";
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

        for (auto& mesh_path : mesh_directory({ wcrop, hcrop })) {
            assert(is_directory(mesh_path));
            const auto mp4_directory = mesh_path / "mp4";
            const auto dash_directory = mesh_path / "dash";
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

    using tinyxml2::XMLNode;
    using tinyxml2::XMLDocument;
    using tinyxml2::XMLElement;
    using tinyxml2::XMLAttribute;
    using tinyxml2::XMLDeclaration;
    using tinyxml2::XMLPrinter;

    static_assert(!std::is_copy_constructible<XMLDocument>::value);
    static_assert(!std::is_move_constructible<XMLDocument>::value);

    auto node_range = [](XMLDocument& document,
                         std::initializer_list<std::string> node_path) {
        return std::reduce(node_path.begin(), node_path.end(), std::vector<XMLElement*>{},
                           [&document](std::vector<XMLElement*>&& container,
                                       const std::string& node_name) {
                               auto* parent = std::empty(container)
                                                  ? std::addressof<XMLNode>(document)
                                                  : container.back();
                               container.push_back(parent->FirstChildElement(node_name.data()));
                               return container;
                           });
    };

    const auto clone_element_if_null = [](XMLDocument& dest_document,
                                          XMLDocument& src_document) {
        return [&dest_document, &src_document](XMLElement*& dest_element,
                                               std::initializer_list<std::string> node_path) {
            if (!dest_element) {
                XMLNode* src_node = &src_document;
                XMLNode* dest_node = &dest_document;
                const auto last_node_name = std::for_each_n(
                    node_path.begin(), node_path.size() - 1,
                    [&dest_node, &src_node](const std::string& node_name) mutable {
                        src_node = src_node->FirstChildElement(node_name.data());
                        if (auto* temp_node = dest_node->FirstChildElement(node_name.data()); temp_node) {
                            dest_node = temp_node;
                        } else {
                            dest_node = dest_node->InsertEndChild(src_node->ShallowClone(dest_node->GetDocument()));
                        }
                    });
                src_node = src_node->FirstChildElement(last_node_name->data());
                dest_node = dest_node->InsertEndChild(src_node->DeepClone(&dest_document));
                assert(src_node&&dest_node);
                return std::exchange(dest_element, dynamic_cast<XMLElement*>(dest_node));
            }
            return dest_element;
        };
    };

    auto spatial_relationship = [](const filter_param filter,
                                   const std::pair<int, int> coordinate) {
        std::string srd;
        folly::toAppendDelim(',', 0,
                             coordinate.first, coordinate.second,
                             1, 1, filter.wcrop, filter.hcrop,
                             &srd);
        return srd;
    };

    void command::merge_dash_mpd() {
        XMLDocument dest_document;
        XMLDeclaration* declaration = nullptr;
        XMLElement* program_information = nullptr;
        auto xml_check = folly::lazy([] {
            return (core::check[tinyxml2::XML_SUCCESS]);
        });
        auto represent_index = 0;
        auto dest_mpd_path = core::file_path_of_directory(output_directory, ".mpd");
        core::make_empty_directory(dest_mpd_path
                                   .replace_filename(
                                       std::filesystem::path{
                                           fmt::format("{}x{}", wcrop, hcrop)
                                       } / dest_mpd_path.filename())
                                   .parent_path());
        for (auto& [coordinate, mpd_paths] : tile_mpd_path_map({ wcrop, hcrop })) {
            XMLElement* adaptation_set = nullptr;
            for (auto& mpd_path : mpd_paths) {
                XMLDocument src_document;
                xml_check() << src_document.LoadFile(mpd_path.string().data());
                if (!declaration) {
                    declaration = src_document.FirstChild()
                                              ->ToDeclaration();
                    dest_document.InsertFirstChild(declaration->ShallowClone(&dest_document));
                }
                auto exchanged_element = clone_element_if_null(dest_document, src_document);
                if (!exchanged_element(program_information, { "MPD", "ProgramInformation" })) {
                    program_information->FirstChildElement("Title")
                                       ->SetText(dest_mpd_path.filename().string().data());
                }
                if (exchanged_element(adaptation_set, { "MPD", "Period", "AdaptationSet" })) {
                    adaptation_set->InsertEndChild(node_range(src_document, { "MPD", "Period", "AdaptationSet", "Representation" })
                                                   .back()
                                                   ->DeepClone(&dest_document));
                } else {
                    auto* supplemental = dest_document.NewElement("SupplementalProperty");
                    supplemental->SetAttribute("schemeIdUri", "urn:mpeg:dash:srd:2014");
                    supplemental->SetAttribute("value", spatial_relationship({ wcrop, hcrop }, coordinate).data());
                    adaptation_set->InsertFirstChild(supplemental);
                }
                adaptation_set->LastChildElement("Representation")
                              ->SetAttribute("id", ++represent_index);
            }
        }
        xml_check() << dest_document.SaveFile(dest_mpd_path.string().data());
    }
}
