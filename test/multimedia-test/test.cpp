#include "pch.h"

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

TEST(Boost, BeastBuffer) {
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

TEST(Core, MultiBuffer2List) {
    multi_buffer buf_int = create_buffer("init");
    multi_buffer buf1 = create_buffer("1");
    auto list = split_buffer_sequence(buf1);
    auto list2 = split_buffer_sequence(buf_int, buf1);
    auto list3 = core::split_buffer_sequence(buf_int, buf1);
    EXPECT_EQ(total_size<std::list>(list), 2090);
    EXPECT_EQ(total_size<std::list>(list2), 2966);
    EXPECT_EQ(total_size<std::list>(list3), 2966);
}

TEST(MultiMedia, Component) {

}