#include "pch.h"
#include "multimedia/component.h"
#include <boost/beast/core/buffers_prefix.hpp>
#include <boost/beast/core/buffers_cat.hpp>
#include <boost/beast/core/ostream.hpp>


using boost::beast::multi_buffer;
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

TEST(Beast, MultiBuffer) {
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

TEST(Asio, BufferSize) {
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

TEST(Core, Split2Sequence) {
    multi_buffer buf_int = create_buffer("init");
    multi_buffer buf1 = create_buffer("1");
    auto list = split_buffer_sequence(buf1);
    auto list2 = split_buffer_sequence(buf_int, buf1);
    auto list3 = core::split_buffer_sequence(buf_int, buf1);
    EXPECT_EQ(total_size<std::list>(list), 2090);
    EXPECT_EQ(total_size<std::list>(list2), 2966);
    EXPECT_EQ(total_size<std::list>(list3), 2966);
}


#pragma comment(lib,"multimedia")
TEST(FrameSegmentor, Base) {
    auto& buffer_map = create_buffer_map();
    for (auto&[index, buffer] : buffer_map) {
        media::component::frame_segmentor frame_segmentor;
        frame_segmentor.parse_context(core::split_buffer_sequence(buffer));
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
    frame_segmentor.parse_context(std::move(buffer_list));
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
    frame_segmentor.parse_context(std::move(buffer_list));
    while (frame_segmentor.try_read()) {
        count1 += 1;
    }
    auto buffer_list2 = core::split_buffer_sequence(buffer_map[0], buffer_map[2], buffer_map[4], buffer_map[6]);
    media::component::frame_segmentor frame_segmentor2;
    auto count2 = 0;
    frame_segmentor2.parse_context(std::move(buffer_list2));
    while (frame_segmentor2.try_read()) {
        count2 += 1;
    }
}

TEST(FrameSegmentor, TryConsume) {
    auto& buffer_map = create_buffer_map();
    media::component::frame_segmentor frame_segmentor{
        core::split_buffer_sequence(buffer_map[0],buffer_map[2],buffer_map[4],buffer_map[6],buffer_map[7],buffer_map[10])
    };
    auto count = 0;
    auto increment = 0;
    do {
        increment = frame_segmentor.try_consume();
        count += increment > 0 ? increment : 0;
    } while (increment >= 0);
    EXPECT_EQ(count, 125);
}

TEST(FrameSegmentor, TryConsumeOnce) {
    auto& buffer_map = create_buffer_map();
    media::component::frame_segmentor frame_segmentor{
        core::split_buffer_sequence(buffer_map[0],buffer_map[2],buffer_map[4],buffer_map[6],buffer_map[7],buffer_map[10])
    };
    auto count = 0;
    while (frame_segmentor.try_consume_once() && ++count) {
    }
    EXPECT_EQ(count, 125);
}

TEST(Std, WeakPtr) {
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

TEST(Std, ListIterator) {
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
