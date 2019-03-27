#include "pch.h"
#include "multimedia/command.h"
#include "multimedia/context.h"
#include "multimedia/io.segmentor.h"
#include "multimedia/media.h"
#include "core/exception.hpp"
#include <folly/executors/Async.h>
#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/executors/GlobalExecutor.h>
#include <folly/executors/task_queue/UnboundedBlockingQueue.h>
#include <folly/MoveWrapper.h>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/multi_buffer.hpp>
#include <boost/beast/core/ostream.hpp>
#include <boost/container/small_vector.hpp>
#include <any>
#include <numeric>

using boost::beast::multi_buffer;
using boost::beast::flat_buffer;
using boost::beast::buffers_prefix;
using boost::beast::ostream;
using boost::beast::read_size;
using boost::beast::read_size_or_throw;
using boost::asio::const_buffer;
using boost::asio::buffer_copy;
using boost::asio::buffer_size;
using boost::asio::buffer_sequence_begin;
using boost::asio::buffer_sequence_end;
using boost::asio::dynamic_buffer;

auto fill_buffer = [](multi_buffer& buffer,
                      std::string suffix) {
    auto path_pattern = suffix == "init"
                            ? "D:/Media/dash/tile1-576p-5000kbps_dash{}.mp4"
                            : "D:/Media/dash/tile1-576p-5000kbps_dash{}.m4s";
    ostream(buffer) << std::ifstream{ fmt::format(path_pattern, suffix), std::ios::binary }.rdbuf();
};

auto create_buffer = [](std::string suffix) {
    multi_buffer buffer;
    fill_buffer(buffer, suffix);
    return buffer;
};

auto create_buffer_map = []() -> decltype(auto) {
    static std::map<int, multi_buffer> map;
    static std::once_flag flag;
    std::call_once(flag,
                   [] {
                       map.emplace(0, create_buffer("init"));
                       for (auto i = 1; i <= 10; ++i) {
                           map.emplace(i, create_buffer(std::to_string(i)));
                       }
                   });
    return (map);
};

static_assert(std::is_reference<std::invoke_result<decltype(create_buffer_map)>::type>::value);

auto sequence_size = [](auto itbegin, auto itend) {
    auto size = 0i64;
    while (itbegin != itend) {
        size += buffer_size(*itbegin);
        ++itbegin;
    }
    return size;
};

auto sequence_size2 = [](auto itbegin, auto itend) {
    auto size = 0i64;
    while (itbegin != itend) {
        size += const_buffer{ *itbegin }.size();
        ++itbegin;
    }
    return size;
};

namespace boost::test
{
    TEST(MultiBuffer, BufferSize) {
        multi_buffer buf_init;
        multi_buffer buf1;
        multi_buffer buf2;
        multi_buffer buf3;
        fill_buffer(buf_init, "init");
        fill_buffer(buf1, "1");
        fill_buffer(buf2, "2");
        fill_buffer(buf3, "3");
        EXPECT_EQ(buffer_size(buf_init.data()), 876);
        EXPECT_EQ(buffer_size(buf1.data()), 2090);
        EXPECT_EQ(buffer_size(buf2.data()), 1417);
        EXPECT_EQ(buffer_size(buf3.data()), 1417);
    }

    TEST(MultiBuffer, BufferSizeMap) {
        auto& buffer_map = create_buffer_map();
        auto size = 0i64;
        for (auto& [index, buffer] : buffer_map) {
            size += buffer_size(buffer.data());
            EXPECT_EQ(&buffer, &buffer_map[index]);
        }
        EXPECT_EQ(size, 11'457'930);
    }
}

template <template<typename> typename Container>
int64_t total_size(Container<const_buffer>& container) {
    size_t size = 0;
    for (auto& element : container) {
        size += element.size();
    }
    return size;
}

void set_cpu_executor(int concurrency) {
    static auto executor = std::make_shared<folly::CPUThreadPoolExecutor>(
        std::make_pair(concurrency, 1),
        std::make_unique<folly::UnboundedBlockingQueue<folly::CPUThreadPoolExecutor::CPUTask>>(),
        std::make_shared<folly::NamedThreadFactory>("TestPool"));
    folly::setCPUExecutor(executor);
}

namespace core::test
{
    TEST(BufferOperation, Split2Sequence) {
        multi_buffer buf_int = create_buffer("init");
        multi_buffer buf1 = create_buffer("1");
        auto list = core::split_buffer_sequence(buf1);
        auto list2 = core::split_buffer_sequence(buf_int, buf1);
        auto list3 = core::split_buffer_sequence(buf_int, buf1);
        EXPECT_EQ(total_size<std::list>(list), 2090);
        EXPECT_EQ(total_size<std::list>(list2), 2966);
        EXPECT_EQ(total_size<std::list>(list3), 2966);
    }
}

namespace media::test
{
    TEST(FrameSegmentor, Base) {
        auto& buffer_map = create_buffer_map();
        for (auto& [index, buffer] : buffer_map) {
            media::frame_segmentor frame_segmentor;
            frame_segmentor.parse_context(core::split_buffer_sequence(buffer), 8);
            EXPECT_FALSE(frame_segmentor.buffer_available());
            break;
        }
    }

    TEST(FrameSegmentor, ReadThree) {
        auto& buffer_map = create_buffer_map();
        auto buffer_list = core::split_buffer_sequence(buffer_map[0], buffer_map[3], buffer_map[5], buffer_map[7]);
        media::frame_segmentor frame_segmentor;
        auto count1 = 0;
        frame_segmentor.parse_context(std::move(buffer_list), 8);
        while (frame_segmentor.try_read()) {
            count1 += 1;
        }
        auto buffer_list2 = core::split_buffer_sequence(buffer_map[0], buffer_map[2], buffer_map[4], buffer_map[6]);
        media::frame_segmentor frame_segmentor2;
        auto count2 = 0;
        frame_segmentor2.parse_context(std::move(buffer_list2), 8);
        while (frame_segmentor2.try_read()) {
            count2 += 1;
        }
    }

    TEST(FrameSegmentor, TryConsume) {
        auto& buffer_map = create_buffer_map();
        media::frame_segmentor frame_segmentor{
            core::split_buffer_sequence(buffer_map[0], buffer_map[2], buffer_map[4],
                                        buffer_map[6], buffer_map[7], buffer_map[10]),
            4
        };
        auto count = 0ui64;
        auto increment = 0ui64;
        try {
            do {
                auto frames = frame_segmentor.try_consume();
                increment = frames.size();
                count += increment > 0 ? increment : 0;
            } while (increment >= 0);
        } catch (core::stream_drained_error) {}
        EXPECT_EQ(count, 125);
    }

    TEST(FrameSegmentor, TryConsumeOnce) {
        auto& buffer_map = create_buffer_map();
        media::frame_segmentor frame_segmentor{
            core::split_buffer_sequence(buffer_map[0], buffer_map[2], buffer_map[4],
                                        buffer_map[6], buffer_map[7], buffer_map[10]),
            4
        };
        auto count = 0;
        while (!frame_segmentor.try_consume_once().empty() && ++count) {}
        EXPECT_EQ(count, 125);
    }
}

auto create_buffer_from_path = [](std::string path) {
    EXPECT_TRUE(std::filesystem::is_regular_file(path));
    multi_buffer buffer;
    ostream(buffer) << std::ifstream{ path, std::ios::binary }.rdbuf();
    return buffer;
};

namespace media::test
{
    TEST(FrameSegmentor, TryConsumeOnceForLargeFile) {
        auto init_buffer = create_buffer_from_path("D:/Media/dash/full/tile1-576p-5000kbps_dashinit.mp4");
        auto tail_buffer = create_buffer_from_path("D:/Media/dash/full/tile1-576p-5000kbps_dash9.m4s");
        media::frame_segmentor fs1{ core::split_buffer_sequence(init_buffer, tail_buffer), 4 };
        std::any a1{ std::move(init_buffer) };
        std::any a2{ std::move(tail_buffer) };
        EXPECT_EQ(init_buffer.size(), 0);
        EXPECT_EQ(tail_buffer.size(), 0);
        auto count = 0;
        while (!fs1.try_consume_once().empty() && ++count) {}
        EXPECT_EQ(count, 25);
    }
}

using frame_consumer = folly::Function<bool()>;
using frame_builder = folly::Function<frame_consumer(std::list<const_buffer>)>;

frame_builder create_frame_builder() {
    return [](std::list<const_buffer> buffer_list) -> frame_consumer {
        auto segmentor = folly::makeMoveWrapper(
            media::frame_segmentor{ std::move(buffer_list), 4 });
        return [segmentor]() mutable {
            return !segmentor->try_consume_once().empty();
        };
    };
}

folly::Future<bool> async_consume(media::frame_segmentor& segmentor,
                                  media::pixel_consume& consume,
                                  bool copy = false) {
    if (copy) {
        return folly::async([&segmentor, &consume]() mutable {
            return segmentor.try_consume_once(consume);
        });
    }
    return folly::async([&segmentor, &consume] {
        return segmentor.try_consume_once(consume);
    });
}

frame_builder create_async_frame_builder(media::pixel_consume& consume) {
    return [&consume](std::list<const_buffer> buffer_list) -> frame_consumer {
        auto segmentor = folly::makeMoveWrapper(
            media::frame_segmentor{ std::move(buffer_list), 4 });
        auto decode = folly::makeMoveWrapper(async_consume(*segmentor, consume, true));
        return [segmentor, decode, &consume]() mutable {
            const auto result = std::move(*decode).get();
            *decode = async_consume(*segmentor, consume);
            return result;
        };
    };
}

namespace media::test
{
    TEST(Frame, Empty) {
        media::frame f;
        EXPECT_TRUE(f.empty());
        EXPECT_FALSE(f.operator->() == nullptr);
        auto f2 = std::move(f);
        EXPECT_TRUE(f.operator->() == nullptr);
        EXPECT_TRUE(f2.empty());
        EXPECT_FALSE(f2.operator->() == nullptr);
    }

    TEST(FrameSegmentor, TransformBuilder) {
        auto& buffer_map = create_buffer_map();
        auto frame_builder = create_frame_builder();
        auto frame_consumer = frame_builder(core::split_buffer_sequence(buffer_map[0], buffer_map[2], buffer_map[4],
                                                                        buffer_map[6], buffer_map[7], buffer_map[10]));
        auto count = 0;
        while (frame_consumer() && ++count) {}
        EXPECT_EQ(count, 125);
    }

    TEST(FrameSegmentor, TransformAsyncBuilder) {
        set_cpu_executor(4);
        auto& buffer_map = create_buffer_map();
        auto count = 0;
        media::pixel_consume consume = [&count](media::pixel_array) {
            count++;
        };
        auto frame_builder = create_async_frame_builder(consume);
        auto frame_consumer = frame_builder(core::split_buffer_sequence(buffer_map[0], buffer_map[2], buffer_map[4],
                                                                        buffer_map[6], buffer_map[7], buffer_map[10]));
        while (frame_consumer()) {}
        EXPECT_EQ(count, 125);
    }
}

std::map<int, multi_buffer> create_tile_buffer_map(std::string prefix, int first, int last) {
    std::map<int, multi_buffer> map;
    ostream(map[0]) << std::ifstream{ fmt::format("{}init.mp4", prefix), std::ios::binary }.rdbuf();
    auto index = first;
    while (index <= last) {
        ostream(map[index]) << std::ifstream{ fmt::format("{}{}.m4s", prefix, index), std::ios::binary }.rdbuf();
        index++;
    }
    EXPECT_EQ(map.size(), last - first + 2);
    for (auto& [index, buffer] : map) {
        fmt::print("{}: {}\n", index, buffer.size());
        EXPECT_TRUE(buffer.size() > 0);
    }
    return map;
}

namespace media::test
{
    TEST(FrameSegmentor, CreateTileBufferMap) {
        auto map = create_tile_buffer_map("F:/Output/NewYork/4x4_1000/dash/NewYork_c3r1_1000kbps_dash", 1, 50);
        std::ofstream mp4{ "F:/Debug/test.mp4", std::ios::out | std::ios::binary | std::ios::trunc };
        std::ofstream yuv{ "F:/Debug/test.yuv", std::ios::out | std::ios::binary | std::ios::trunc };
        for (auto& [index, buffer] : map) {
            if (index > 0) {
                auto count = 0i64;
                for (auto sub_buf : buffer.data()) {
                    mp4.write(static_cast<const char*>(sub_buf.data()), sub_buf.size());
                    EXPECT_TRUE(mp4.good());
                }
                media::frame_segmentor segmentor{
                    core::split_buffer_sequence(map.at(0), map.at(index)),
                    4
                };
                auto width = 0, height = 0;
                while (segmentor.codec_available()) {
                    auto frames = segmentor.try_consume();
                    count += std::size(frames);
                    for (auto& frame : frames) {
                        yuv.write(reinterpret_cast<const char*>(frame->data[0]), frame->width * frame->height);
                        yuv.write(reinterpret_cast<const char*>(frame->data[1]), frame->width * frame->height / 4);
                        yuv.write(reinterpret_cast<const char*>(frame->data[2]), frame->width * frame->height / 4);
                        if (!width || !height) {
                            width = frame->width;
                            height = frame->height;
                        }
                    }
                }
                fmt::print("width {} height {}\n", width, height);
                fmt::print("{}: count {}\n", index, count);
                EXPECT_EQ(count, 60);
            }
        }
    }
}

auto make_even = [](int num) constexpr {
    return num % 2 != 0 ? num + 1 : num;
};

void scale_partial(const std::string_view input,
                   const unsigned wcrop, const unsigned hcrop,
                   const unsigned wscale, const unsigned hscale,
                   const unsigned stride = 16, const unsigned offset = 0) {
    if (offset >= wcrop * hcrop) {
        return;
    }
    auto kbyte = 0;
    auto mbit = kbyte * 8 / 1000;
    create_directories(std::filesystem::path{ "F:/Gpac/debug" } / std::filesystem::path{ std::string{ input } }.stem());
    auto cmd = fmt::format("ffmpeg -i {} -filter_complex \"", input);
    const auto [width, height] = media::format_context{ media::source::path{ input.data() } }.demux(media::type::video).scale();
    const auto scale = fmt::format("scale={}:{}", make_even(width / wcrop / wscale), make_even(height / hcrop / hscale));
    for (auto i = 0; i != wcrop; ++i) {
        for (auto j = 0; j != hcrop; ++j) {
            if (i * hcrop + j < offset || i * hcrop + j >= offset + stride)
                continue;
            const auto crop = fmt::format("crop={}:{}:{}:{}",
                                          width / wcrop, height / hcrop,
                                          width * i / wcrop, height * j / hcrop);
            cmd.append(fmt::format("[0:v]{},{}[v{}:{}];", crop, scale, i, j));
        }
    }
    cmd.pop_back();
    cmd.append("\" ");
    auto filename = std::filesystem::path{ std::string{ input } }.stem().generic_string();
    for (auto i = 0; i != wcrop; ++i) {
        for (auto j = 0; j != hcrop; ++j) {
            if (i * hcrop + j < offset || i * hcrop + j >= offset + stride)
                continue;
            if (wscale > 1 || hscale > 1) {
                cmd.append(fmt::format("-map \"[v{}:{}]\" -c:v h264 E:/Tile/{}/t{}_{}_{}_{}.mp4 ", i, j,
                                       std::filesystem::path{ std::string{ input } }.stem().generic_string(), wcrop * hcrop, wscale * hscale, j, i));
            } else {
                cmd.append(fmt::format(
                    "-map \"[v{}:{}]\" -c:v libx264  -preset slow "
                    "-x264-params keyint=30:min-keyint=30:bitrate=5000:vbv-maxrate=10000:vbv-bufsize=20000:fps=30:scenecut=0:no-scenecut:pass=1 "
                    "F:/Gpac/debug/{}/{}_{}_{}_5K.mp4 ",
                    i, j,
                    filename, filename, j, i));
            }
        }
    }
    cmd.append(" -y");
    std::system(cmd.data());
    scale_partial(input, wcrop, hcrop, wscale, hscale, stride, offset + stride);
}

class CommandBase : public testing::Test
{
protected:
    //inline static const std::filesystem::path output_directory = "F:/Output";
    inline static const std::vector<int> video_index{ 0 };
    inline static const std::map<int, std::filesystem::path> input_video_path_map{
        { 0, "F:/Gpac/NewYork.mp4" }
    };
    std::vector<std::filesystem::path> video_paths_;

    void SetUp() override {
        video_paths_ = std::reduce(
            video_index.begin(), video_index.end(),
            std::vector<std::filesystem::path>{},
            [this](std::vector<std::filesystem::path>&& paths,
                   const int path_index) {
                paths.push_back(input_video_path_map.at(path_index));
                return paths;
            });
        ASSERT_FALSE(std::empty(video_paths_));
        MakeDirectory();
    }

    void MakeDirectory(std::filesystem::path output_directory = "F:/Output") {
        ASSERT_TRUE(is_directory(output_directory.root_directory()));
        create_directories(output_directory);
        ASSERT_TRUE(is_directory(output_directory));
        media::command::output_directory = output_directory;
    };
};

namespace media::test
{
    TEST_F(CommandBase, Resize) {
        media::command::resize("F:/Gpac/NewYork.mp4", { 1920, 1080 });
    }

    TEST_F(CommandBase, Scale) {
        scale_partial("F:/Gpac/NewYork.mp4", 3, 3, 1, 1);
    }

    TEST_F(CommandBase, CropScaleMedia3x3) {
        media::command::crop_scale_transcode("F:/Gpac/NewYork.mp4", { 3, 3 }, { 5000, 60 });
        media::command::crop_scale_transcode("F:/Gpac/NewYork.mp4", { 3, 3 }, { 2500, 60 });
        media::command::crop_scale_transcode("F:/Gpac/NewYork.mp4", { 3, 3 }, { 1000, 60 });
    }

    TEST_F(CommandBase, CropScaleMedia4x4) {
        media::command::crop_scale_transcode("F:/Gpac/NewYork.mp4", { 4, 4 }, { 3000 });
        media::command::crop_scale_transcode("F:/Gpac/NewYork.mp4", { 4, 4 }, { 2000 });
        media::command::crop_scale_transcode("F:/Gpac/NewYork.mp4", { 4, 4 }, { 1000 });
    }

    TEST_F(CommandBase, PackageMp4) {
        MakeDirectory("F:/Output/NewYork/");
        media::command::package_container({ -1, 60 });
    }

    TEST_F(CommandBase, DashSegmental) {
        MakeDirectory("F:/Output/NewYork/");
        media::command::dash_segment(1000ms);
    }

    TEST_F(CommandBase, MergeDashMpd) {
        MakeDirectory("F:/Output/NewYork/");
        media::command::merge_dash_mpd();
    }

    auto command_environment = [](std::filesystem::path output_directory,
                                  std::pair<int, int> crop) {
        ASSERT_TRUE(is_directory(output_directory.root_directory()));
        create_directories(output_directory);
        ASSERT_TRUE(is_directory(output_directory));
        media::command::output_directory = output_directory;
        media::command::wcrop = crop.first;
        media::command::hcrop = crop.second;
    };

    TEST(Pipeline, NewYork5x4) {
        command_environment("F:/Output/", { 5, 4 });
        media::command::crop_scale_package("F:/Gpac/NewYork.mp4", { 3000 });
        media::command::crop_scale_package("F:/Gpac/NewYork.mp4", { 2000 });
        media::command::crop_scale_package("F:/Gpac/NewYork.mp4", { 1000 });
        command_environment("F:/Output/NewYork/", { 5, 4 });
        media::command::package_container({ -1, 60 });
        media::command::dash_segment(1000ms);
        media::command::merge_dash_mpd();
    }

    auto command_rate_batch = [](std::filesystem::path input,
                                 std::filesystem::path output_directory,
                                 bool trancode = true) {
        EXPECT_TRUE(std::filesystem::is_regular_file(input));
        EXPECT_TRUE(std::filesystem::is_directory(output_directory));
        return [input, output_directory, trancode](std::pair<int, int> crop,
                                                   std::initializer_list<int> bitrates,
                                                   std::chrono::milliseconds duration = 1000ms) {
            auto [wcrop, hcrop] = crop;
            EXPECT_GT(wcrop, 0);
            EXPECT_GT(hcrop, 0);
            if (trancode) {
                command_environment(output_directory, crop);
                for (const auto bitrate : bitrates) {
                    media::command::crop_scale_transcode(input, { bitrate, 60 });
                }
            }
            command_environment(output_directory / input.stem(), crop);
            if (trancode) {
                media::command::package_container({ -1, 60 });
            }
            media::command::dash_segment(duration);
            media::command::merge_dash_mpd();
        };
    };

    auto command_qp_batch = [](std::filesystem::path input,
                               std::filesystem::path output_directory,
                               std::filesystem::path copy_directory) {
        EXPECT_TRUE(std::filesystem::is_regular_file(input));
        EXPECT_TRUE(std::filesystem::is_directory(output_directory));
        return [=](std::pair<int, int> crop,
                   std::initializer_list<int> qp_list,
                   std::chrono::milliseconds duration = 1000ms) {
            auto [wcrop, hcrop] = crop;
            EXPECT_GT(wcrop, 0);
            EXPECT_GT(hcrop, 0);
            command_environment(output_directory, crop);
            for (const auto qp : qp_list) {
                media::command::crop_scale_package(input, qp);
            }
            command_environment(output_directory / input.stem(), crop);
            media::command::dash_segment(duration);
            auto mpd_path = media::command::merge_dash_mpd();
            const auto target_directory = copy_directory / input.stem() / fmt::format("{}x{}", wcrop, hcrop);
            create_directories(target_directory);
            copy_file(mpd_path, target_directory / mpd_path.filename(),
                      std::filesystem::copy_options::overwrite_existing);
            auto tile_path_list = media::command::tile_path_list();
            for (auto& tile_path : tile_path_list) {
                copy_file(tile_path, target_directory / tile_path.filename(),
                          std::filesystem::copy_options::overwrite_existing);
            }
        };
    };

    TEST(CommandBatch, NewYorkRateBatch) {
        const auto command = command_rate_batch("F:/Gpac/NewYork.mp4", "F:/Output/", true);
        command({ 8, 4 }, { 1000, 800, 600, 400, 200 });
        command({ 5, 3 }, { 2000, 1500, 1000, 500, 200 });
        command({ 5, 4 }, { 1500, 1200, 1000, 500, 200 });
        command({ 3, 3 }, { 3000, 2000, 1000, 500, 200 });
        command({ 4, 3 }, { 2000, 1500, 1000, 500, 200 });
    }

    TEST(CommandBatch, NewYorkQpBatch) {
        auto command = command_qp_batch("F:/Gpac/NewYork.mp4",
                                        "F:/Output/",
                                        "D:/Media");
        command({ 6, 5 }, { 22, 27, 32, 37, 42 });
        command({ 3, 3 }, { 22, 27, 32, 37, 42 });
        command({ 5, 3 }, { 22, 27, 32, 37, 42 });
        command({ 4, 3 }, { 22, 27, 32, 37, 42 });
        command({ 5, 4 }, { 22, 27, 32, 37, 42 });
    }

    TEST(CommandBatch, AngelFallsVenezuelaQpBatch) {
        auto command = command_qp_batch("E:/VR/AngelFallsVenezuela7680x3840.mkv",
                                        "F:/Output/",
                                        "D:/Media");
        command({ 3, 3 }, { 22, 32, 42 });
    }

    TEST(Command, CropScaleTranscodeByQp) {
        media::command::output_directory = "F:/Debug";
        media::command::wcrop = 3;
        media::command::hcrop = 3;
        media::command::crop_scale_package("F:/Gpac/NewYork.mp4", 22);
    }

    TEST(Command, SegmentDash) {
        media::command::dash_segment("F:/Debug/NewYork_c1r0_qp22.mp4", 1000ms);
    }
}
