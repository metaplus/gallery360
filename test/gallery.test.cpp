#include "pch.h"
#include "network/dash.manager.h"
#include "multimedia/io.segmentor.h"
#include "gallery/pch.h"
#include "gallery/database.sqlite.h"
#include <folly/MoveWrapper.h>
#include <folly/Try.h>
#include <boost/beast/core/multi_buffer.hpp>
#include <boost/container/flat_map.hpp>
#include <objbase.h>
#include "core/meta/exception_trait.hpp"
#include <range/v3/view/cartesian_product.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/action/remove_if.hpp>

using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::steady_clock;
using boost::asio::const_buffer;
using boost::beast::multi_buffer;
using net::dash_manager;
using media::frame_segmentor;
using media::pixel_array;
using media::pixel_consume;

using frame_consumer = folly::Function<bool()>;
using frame_indexed_builder = folly::Function<
    frame_consumer(std::pair<int, int>,
                   boost::beast::multi_buffer&,
                   boost::beast::multi_buffer&&)>;

using namespace unity;

namespace tuning
{
    constexpr auto verbose = false;
    constexpr auto profile = false;
}

frame_indexed_builder create_frame_builder(pixel_consume& consume,
                                           unsigned concurrency = std::thread::hardware_concurrency()) {
    return [&consume, concurrency](auto ordinal,
                                   multi_buffer& head_buffer,
                                   multi_buffer&& tail_buffer)
    -> frame_consumer {
        auto segmentor = folly::makeMoveWrapper(
            frame_segmentor{ core::split_buffer_sequence(head_buffer, tail_buffer), concurrency });
        auto tail_buffer_wrapper = folly::makeMoveWrapper(tail_buffer);
        auto decode = folly::makeMoveWrapper(segmentor->defer_consume_once(consume));
        return [segmentor, decode, tail_buffer_wrapper, &consume]() mutable {
            decode->wait();
            if (decode->hasValue()) {
                auto defer_execute = decode.move().get();
                *decode = segmentor->defer_consume_once(consume);
                defer_execute();
                return false;
            }
            return true;
        };
    };
}

auto plugin_routine = [](std::string url) {
    return [url](unsigned codec_concurrency = 8) {
        std::this_thread::sleep_for(100ms);
        ASSERT_TRUE(_nativeLibraryConfigLoad("http://47.101.209.146:33666/NewYork/3x3/NewYork.mpd"));
        test::_nativeTestConcurrencyStore(1, 2);
        _nativeLibraryInitialize();
        _nativeDashCreate();
        int col = 0, row = 0, width = 0, height = 0;
        ASSERT_TRUE(_nativeDashGraphicInfo(col, row, width, height));
        ASSERT_EQ(col, 3);
        ASSERT_EQ(row, 3);
        ASSERT_EQ(width, 3840);
        ASSERT_EQ(height, 1920);
        std::vector<void*> stream_list;
        std::map<void*, core::coordinate> stream_coordinate_map;
        for (auto [c,r] : ranges::view::cartesian_product(
                 ranges::view::ints(0, col), ranges::view::ints(0, row))) {
            auto index = 1 + c + col * r;
            auto* tile_stream = _nativeDashCreateTileStream(c, r, index, nullptr, nullptr, nullptr);
            stream_coordinate_map[tile_stream] = core::coordinate{ c, r };
            stream_list.push_back(tile_stream);
        }
        ASSERT_EQ(col*row, stream_coordinate_map.size());
        folly::stop_watch<seconds> watch;
        _nativeDashPrefetch();
        auto stream_cache = stream_list;
        struct counter
        {
            int frame = 0;
            int tile = 0;
        } counters;
        auto available = true;
        auto update_time = std::chrono::steady_clock::now() + 10ms;
        while (available) {
            std::this_thread::sleep_until(update_time);
            update_time += 20ms;
            stream_cache |= ranges::action::remove_if(
                [&](void* tile_stream) {
                    const auto poll_success =
                        _nativeDashTilePtrPollUpdate(tile_stream, counters.frame, counters.tile);
                    if (poll_success) {
                        counters.tile++;
                    }
                    return poll_success;
                });
            if (stream_cache.empty()) {
                if (++counters.frame % 100 == 0 || counters.frame > 3700) {
                    XLOG(INFO) << "frame " << counters.frame;
                }
                //EXPECT_EQ(counters.tile % 9, 0);
                stream_cache = stream_list;
                available = _nativeDashAvailable();
            }
        }
        const auto t1 = watch.elapsed();
        XLOG(INFO) << "-- profile parting line\n"
            << "concurrency " << codec_concurrency << "\n"
            << "iteration " << counters.frame << "\n"
            << "time " << t1.count() << "\n"
            << "fps " << counters.frame / t1.count() << "\n";
        _nativeLibraryRelease();
        EXPECT_GE(counters.frame, 3715);
    };
};

namespace gallery::test
{
    TEST(Plugin, PollLoopProfile) {
        auto profile_codec_concurrency = plugin_routine(
            "http://47.101.209.146:33666/NewYork/3x3/NewYork.mpd");
        if constexpr (tuning::profile) {
            profile_codec_concurrency(8); // fps 154
            profile_codec_concurrency(4); // fps 161
            profile_codec_concurrency(3); // fps 161
            profile_codec_concurrency(2); // fps 168
            profile_codec_concurrency(1); // fps 168
        } else {
            profile_codec_concurrency(1); // fps 154
        }
    }

    TEST(Plugin, PollMockLoop) { }

    TEST(Gallery, Concurrency) {
        unsigned codec = 0, net = 0, executor = 0;
        unity::test::_nativeTestConcurrencyLoad(codec, net, executor);
        EXPECT_EQ(codec, 2);
        EXPECT_EQ(net, 8);
        EXPECT_EQ(executor, 8);
        unity::test::_nativeTestConcurrencyStore(4, 4);
        unity::test::_nativeTestConcurrencyLoad(codec, net, executor);
        EXPECT_EQ(codec, 4);
        EXPECT_EQ(net, 4);
        EXPECT_EQ(executor, 8);
    }

    TEST(Test, ManagedString) {
        auto str = unity::test::_nativeTestString();
        EXPECT_GT(std::strlen(str), 0);
        CoTaskMemFree(str);
    }

    TEST(Unity, LoadEnvConfig) {
        auto* mpd_url = "http://47.101.209.146:33666/NewYork/3x3/NewYork.mpd";
        EXPECT_TRUE(unity::_nativeLibraryConfigLoad(mpd_url));
        _nativeLibraryInitialize();
        const auto str = unity::_nativeDashCreate();
        XLOG(INFO) << str;
        EXPECT_FALSE(std::string_view{str}.empty());
    }
}
