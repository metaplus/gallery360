#include "pch.h"
#include "core/pch.h"
#include "multimedia/component.h"
#include "multimedia/pch.h"
#include <boost/beast/core/buffers_prefix.hpp>
#include <boost/beast/core/buffers_cat.hpp>
#include <boost/beast/core/ostream.hpp>
#include <folly/executors/thread_factory/NamedThreadFactory.h>
#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/executors/task_queue/UnboundedBlockingQueue.h>
#include "multimedia/command.h"

using boost::beast::multi_buffer;
using boost::beast::flat_buffer;
using boost::beast::buffers_cat;
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

auto fill_buffer = [](multi_buffer& buffer, std::string suffix) {
    auto path_pattern = suffix == "init" ?
        "D:/Media/dash/tile1-576p-5000kbps_dash{}.mp4" :
        "D:/Media/dash/tile1-576p-5000kbps_dash{}.m4s";
    ostream(buffer) << std::ifstream{ fmt::format(path_pattern,suffix),std::ios::binary }.rdbuf();
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

TEST(Buffer, MultiBuffer) {
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
    EXPECT_EQ(buffer_size(buffers_cat(buf_init.data(), buf1.data())), 2966);
    EXPECT_EQ(buffer_size(buffers_cat(buf_init.data(), buf1.data(), buf2.data(), buf3.data())), 5800);
    auto buffer_concat = buffers_cat(buf_init.data(), buf1.data(), buf2.data(), buf3.data());
    auto begin = buffer_concat.begin();
    auto end = buffer_concat.end();
    auto dist = std::distance(begin, end);
    EXPECT_EQ(sequence_size(begin, end), 5800);
    EXPECT_EQ(sequence_size2(begin, end), 5800);
}

TEST(Buffer, BufferSize) {
    auto& buffer_map = create_buffer_map();
    auto size = 0i64;
    for (auto&[index, buffer] : buffer_map) {
        size += buffer_size(buffer.data());
        EXPECT_EQ(&buffer, &buffer_map[index]);
    }
    EXPECT_EQ(size, 11'457'930);
}

template<typename BufferSequence, typename ...TailSequence>
std::list<const_buffer> split_buffer_sequence(BufferSequence&& sequence, TailSequence&& ...tails) {
    std::list<const_buffer> buffer_list;
    auto sequence_data = sequence.data();
    std::transform(buffer_sequence_begin(sequence_data),
                   buffer_sequence_end(sequence_data),
                   std::back_inserter(buffer_list),
                   [](const auto& buffer) { return const_buffer{ buffer }; });
    if constexpr (sizeof...(TailSequence) > 0) {
        buffer_list.splice(buffer_list.end(),
                           split_buffer_sequence(std::forward<TailSequence>(tails)...));
    }
    return buffer_list;
}

template<template<typename> typename Container>
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

TEST(Buffer, Split2Sequence) {
    multi_buffer buf_int = create_buffer("init");
    multi_buffer buf1 = create_buffer("1");
    auto list = split_buffer_sequence(buf1);
    auto list2 = split_buffer_sequence(buf_int, buf1);
    auto list3 = core::split_buffer_sequence(buf_int, buf1);
    EXPECT_EQ(total_size<std::list>(list), 2090);
    EXPECT_EQ(total_size<std::list>(list2), 2966);
    EXPECT_EQ(total_size<std::list>(list3), 2966);
}

TEST(Buffer, Concat) {
    multi_buffer buf0 = create_buffer("init");
    multi_buffer buf1 = create_buffer("1");
    EXPECT_EQ(buf0.size(), 876);
    EXPECT_EQ(buf1.size(), 2090);
    auto concat = buffers_cat(buf0.data(), buf1.data());
    auto s = 0i64;
    for (const_buffer& b : concat) {
        s += b.size();
    }
    EXPECT_EQ(s, 2966);
}

TEST(FrameSegmentor, Base) {
    auto& buffer_map = create_buffer_map();
    for (auto&[index, buffer] : buffer_map) {
        media::component::frame_segmentor frame_segmentor;
        frame_segmentor.parse_context(core::split_buffer_sequence(buffer), 8);
        EXPECT_FALSE(frame_segmentor.buffer_available());
        break;
    }
}

TEST(FrameSegmentor, ReadTwo) {
    auto& buffer_map = create_buffer_map();
    auto buffer_list = core::split_buffer_sequence(buffer_map[0]);
    buffer_list.splice(buffer_list.end(), core::split_buffer_sequence(buffer_map[1]));
    media::component::frame_segmentor frame_segmentor;
    auto count1 = 0, count2 = 0;
    frame_segmentor.parse_context(std::move(buffer_list), 8);
    while (frame_segmentor.try_read()) {
        count1 += 1;
    }
    frame_segmentor.reset_buffer_list(core::split_buffer_sequence(buffer_map[2]));
    while (frame_segmentor.try_read()) {
        count2 += 1;
    }
}

TEST(FrameSegmentor, ReadThree) {
    auto& buffer_map = create_buffer_map();
    auto buffer_list = core::split_buffer_sequence(buffer_map[0], buffer_map[3], buffer_map[5], buffer_map[7]);
    media::component::frame_segmentor frame_segmentor;
    auto count1 = 0;
    frame_segmentor.parse_context(std::move(buffer_list), 8);
    while (frame_segmentor.try_read()) {
        count1 += 1;
    }
    auto buffer_list2 = core::split_buffer_sequence(buffer_map[0], buffer_map[2], buffer_map[4], buffer_map[6]);
    media::component::frame_segmentor frame_segmentor2;
    auto count2 = 0;
    frame_segmentor2.parse_context(std::move(buffer_list2), 8);
    while (frame_segmentor2.try_read()) {
        count2 += 1;
    }
}

TEST(FrameSegmentor, TryConsume) {
    auto& buffer_map = create_buffer_map();
    media::component::frame_segmentor frame_segmentor{
        core::split_buffer_sequence(buffer_map[0],buffer_map[2],buffer_map[4],buffer_map[6],buffer_map[7],buffer_map[10])
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
    media::component::frame_segmentor frame_segmentor{
        core::split_buffer_sequence(buffer_map[0],buffer_map[2],buffer_map[4],buffer_map[6],buffer_map[7],buffer_map[10])
    };
    auto count = 0;
    while (!frame_segmentor.try_consume_once().empty() && ++count) {
    }
    EXPECT_EQ(count, 125);
}

auto create_buffer_from_path = [](std::string path) {
    EXPECT_TRUE(std::filesystem::is_regular_file(path));
    multi_buffer buffer;
    ostream(buffer) << std::ifstream{ path,std::ios::binary }.rdbuf();
    return buffer;
};

TEST(FrameSegmentor, TryConsumeOnceForLargeFile) {
    auto init_buffer = create_buffer_from_path("D:/Media/dash/full/tile1-576p-5000kbps_dashinit.mp4");
    auto tail_buffer = create_buffer_from_path("D:/Media/dash/full/tile1-576p-5000kbps_dash9.m4s");
    media::component::frame_segmentor fs1{ core::split_buffer_sequence(init_buffer,tail_buffer) };
    std::any a1{ std::move(init_buffer) };
    std::any a2{ std::move(tail_buffer) };
    EXPECT_EQ(init_buffer.size(), 0);
    EXPECT_EQ(tail_buffer.size(), 0);
    auto count = 0;
    while (!fs1.try_consume_once().empty() && ++count) {
    }
    EXPECT_EQ(count, 25);
}

using frame_consumer = folly::Function<bool()>;
using frame_builder = folly::Function<frame_consumer(std::list<const_buffer>)>;
using media::component::frame_segmentor;
using media::component::pixel_array;
using media::component::pixel_consume;

frame_builder create_frame_builder() {
    return [](std::list<const_buffer> buffer_list) -> frame_consumer {
        auto segmentor = folly::makeMoveWrapper(frame_segmentor{ std::move(buffer_list) });
        return [segmentor]() mutable {
            return !segmentor->try_consume_once().empty();
        };
    };
}

TEST(FrameSegmentor, TransformBuilder) {
    auto& buffer_map = create_buffer_map();
    auto frame_builder = create_frame_builder();
    auto frame_consumer = frame_builder(core::split_buffer_sequence(buffer_map[0], buffer_map[2], buffer_map[4],
                                                                    buffer_map[6], buffer_map[7], buffer_map[10]));
    auto count = 0;
    while (frame_consumer() && ++count) {
    }
    EXPECT_EQ(count, 125);
}

folly::Future<bool> async_consume(frame_segmentor& segmentor,
                                  pixel_consume& consume,
                                  bool copy = false) {
    if (copy) {
        return folly::async([segmentor, &consume]() mutable { return segmentor.try_consume_once(consume); });
    }
    return folly::async([&segmentor, &consume] { return segmentor.try_consume_once(consume); });
}

frame_builder create_async_frame_builder(pixel_consume& consume) {
    return [&consume](std::list<const_buffer> buffer_list) -> frame_consumer {
        auto segmentor = folly::makeMoveWrapper(frame_segmentor{ std::move(buffer_list) });
        auto decode = folly::makeMoveWrapper(async_consume(*segmentor, consume, true));
        return[segmentor, decode, &consume]() mutable {
            const auto result = std::move(*decode).get();
            *decode = async_consume(*segmentor, consume);
            return result;
        };
    };
}

TEST(FrameSegmentor, TransformAsyncBuilder) {
    set_cpu_executor(4);
    auto& buffer_map = create_buffer_map();
    auto count = 0;
    pixel_consume consume = [&count](pixel_array) { count++; };
    auto frame_builder = create_async_frame_builder(consume);
    auto frame_consumer = frame_builder(core::split_buffer_sequence(buffer_map[0], buffer_map[2], buffer_map[4],
                                                                    buffer_map[6], buffer_map[7], buffer_map[10]));
    while (frame_consumer()) {
    }
    EXPECT_EQ(count, 125);
}

TEST(WeakPtr, Expired) {
    std::weak_ptr<int> w;
    EXPECT_TRUE(w.expired());
    w = std::make_shared<int>(1);
    EXPECT_TRUE(w.expired());
    auto&& s = std::make_shared<int>(1);
    w = s;
    EXPECT_TRUE(!w.expired());
    w = std::make_shared<int>();
    EXPECT_TRUE(w.expired());
}

TEST(List, Iterator) {
    std::list<int> l{ 1,2,3 };
    auto iter = l.begin();
    EXPECT_EQ(*iter, 1);
    std::advance(iter, 2);
    EXPECT_EQ(*iter, 3);
    EXPECT_EQ(std::next(iter), l.end());
    l.emplace_back(4);
    l.emplace_front(0);
    EXPECT_NE(std::next(iter), l.end());
    std::advance(iter, 1);
    EXPECT_EQ(*iter, 4);
    EXPECT_EQ(std::next(iter), l.end());
}

TEST(Future, Exchange) {
    auto f = folly::makeFuture(1);
    auto i = std::exchange(f, folly::makeFuture(2)).value();
    EXPECT_EQ(i, 1);
    auto i2 = std::exchange(f, folly::makeFuture(3)).get();
    EXPECT_EQ(i2, 2);
    EXPECT_EQ(f.value(), 3);
}

std::map<int, multi_buffer> create_tile_buffer_map(std::string prefix, int first, int last) {

    std::map<int, multi_buffer> map;
    ostream(map[0]) << std::ifstream{ fmt::format("{}_init.mp4",prefix),std::ios::binary }.rdbuf();
    auto index = first;
    while (index <= last) {
        ostream(map[index]) << std::ifstream{ fmt::format("{}_{}.m4s",prefix,index),std::ios::binary }.rdbuf();
        index++;
    }
    EXPECT_EQ(map.size(), last - first + 2);
    for (auto&[index, buffer] : map) {
        fmt::print("{}: {}\n", index, buffer.size());
        EXPECT_TRUE(buffer.size() > 0);
    }
    return map;
}

TEST(Command, CreateTileBufferMap) {
    auto map = create_tile_buffer_map("D:/Media/dash/NewYork/5k/segment_0_0_5k", 10, 20);
    std::ofstream mp4{ "F:/debug/test.mp4",std::ios::out | std::ios::binary | std::ios::trunc };
    std::ofstream yuv{ "F:/debug/test.yuv",std::ios::out | std::ios::binary | std::ios::trunc };
    for (auto&[index, buffer] : map) {
        if (index > 0) {
            auto count = 0i64;
            for (auto sub_buf : buffer.data()) {
                mp4.write(static_cast<const char*>(sub_buf.data()), sub_buf.size());
                EXPECT_TRUE(mp4.good());
            }
            frame_segmentor segmentor{ core::split_buffer_sequence(map.at(0),map.at(index)) };
            while (segmentor.codec_valid()) {
                auto frames = segmentor.try_consume();
                count += std::size(frames);
                for (auto& frame : frames) {
                    yuv.write(reinterpret_cast<const char*>(frame->data[0]), frame->width*frame->height);
                    yuv.write(reinterpret_cast<const char*>(frame->data[1]), frame->width*frame->height / 4);
                    yuv.write(reinterpret_cast<const char*>(frame->data[2]), frame->width*frame->height / 4);
                }
            }
            fmt::print("{}: count {}\n", index, count);
            EXPECT_EQ(count, 30);
        }
    }
}

auto make_even = [](int num) constexpr {
    return num % 2 != 0 ? num + 1 : num;
};

void scale_partial(const std::string_view input,
                   unsigned wcrop, unsigned hcrop,
                   const unsigned wscale, const unsigned hscale,
                   const unsigned stride = 16, const unsigned offset = 0) {
    if (offset >= wcrop * hcrop) {
        return;
    }
    auto kbyte = 0;
    auto mbit = kbyte * 8 / 1000;
    create_directories(std::filesystem::path{ "F:/Gpac/debug" } / std::filesystem::path{ std::string{ input } }.stem());
    auto cmd = fmt::format("ffmpeg -i {} -filter_complex \"", input);
    const auto[width, height] = media::format_context{ media::source::path{ input.data() } }.demux(media::type::video).scale();
    const auto scale = fmt::format("scale={}:{}", make_even(width / wcrop / wscale), make_even(height / hcrop / hscale));
    for (auto i = 0; i != wcrop; ++i) {
        for (auto j = 0; j != hcrop; ++j) {
            if (i * hcrop + j < offset || i * hcrop + j >= offset + stride)
                continue;
            const auto crop = fmt::format("crop={}:{}:{}:{}",
                                          width / wcrop, height / hcrop,
                                          width*i / wcrop, height*j / hcrop);
            //cmd.append(fmt::format("[0:v]{}, {}[v{}:{}] ", scale, crop, i, j));
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
                                       std::filesystem::path{ std::string{ input } }.stem().generic_string(), wcrop*hcrop, wscale*hscale, j, i));
            } else {
                cmd.append(fmt::format("-map \"[v{}:{}]\" -c:v libx264  -preset slow "
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

TEST(Media, Frame) {
    media::frame f;
    EXPECT_TRUE(f.empty());
    EXPECT_FALSE(f.operator->() == nullptr);
    auto f2 = std::move(f);
    EXPECT_TRUE(f.operator->() == nullptr);
    EXPECT_TRUE(f2.empty());
    EXPECT_FALSE(f2.operator->() == nullptr);
}

auto command_output_directory = [](std::filesystem::path output_directory = "F:/Output") {
    {
        EXPECT_TRUE(is_directory(output_directory.root_directory()));
        create_directories(output_directory);
        EXPECT_TRUE(is_directory(output_directory));
    }
    media::command::output_directory = output_directory;
};

TEST(Command, Resize) {
    command_output_directory();
    media::command::resize("F:/Gpac/NewYork.mp4", { 1920, 1080 });
}

TEST(Command, Scale) {
    command_output_directory();
    scale_partial("F:/Gpac/NewYork.mp4", 3, 3, 1, 1);
}

TEST(Command, ScaleMedia) {
    command_output_directory();
    {
        media::command::crop_scale_transcode("F:/Gpac/NewYork.mp4", { 4,4 }, { 3000 });
        media::command::crop_scale_transcode("F:/Gpac/NewYork.mp4", { 4,4 }, { 2000 });
        media::command::crop_scale_transcode("F:/Gpac/NewYork.mp4", { 4,4 }, { 1000 });
    }
    {
        media::command::crop_scale_transcode("F:/Gpac/NewYork.mp4", { 3,3 }, { 5000 });
        media::command::crop_scale_transcode("F:/Gpac/NewYork.mp4", { 3,3 }, { 2500 });
        media::command::crop_scale_transcode("F:/Gpac/NewYork.mp4", { 3,3 }, { 1000 });
    }
}