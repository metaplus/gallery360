#include "pch.h"
#include "core/pch.h"
#include "network/dash.manager.h"
#include "multimedia/io.segmentor.h"
#include "gallery/pch.h"
#include "gallery/database.sqlite.h"
#include <boost/beast.hpp>
#include <boost/container/flat_map.hpp>
#include <objbase.h>

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
        test::_nativeConfigConcurrency(codec_concurrency);
        test::_nativeMockGraphic();
        _nativeLibraryInitialize();
        _nativeDashCreate();
        int col = 0, row = 0, width = 0, height = 0;
        ASSERT_TRUE(_nativeDashGraphicInfo(col, row, width, height));
        EXPECT_EQ(col, 5);
        EXPECT_EQ(row, 3);
        ASSERT_EQ(width, 3840);
        ASSERT_EQ(height, 1920);
        auto iteration = 0;
        std::vector<int> ref_index_range;
        std::map<int, std::pair<int, int>> coordinate_map;
        for (auto r = 0; r < row; ++r) {
            for (auto c = 0; c < col; ++c) {
                const auto index = r * col + c + 1;
                ref_index_range.push_back(index);
                coordinate_map[index] = std::make_pair(c, r);
                //_nativeDashSetTexture(c, r, index, nullptr, nullptr, nullptr);
            }
        }
        folly::stop_watch<seconds> watch;
        _nativeDashPrefetch();
        auto index_range = ref_index_range;
        auto begin_iter = index_range.begin();
        auto remove_iter = index_range.end();
        auto count = 0;
        while (_nativeDashAvailable()) {
            remove_iter = std::remove_if(
                begin_iter, remove_iter,
                [&](int index) {
                    const auto [c, r] = coordinate_map.at(index);
                    const auto poll_success = _nativeDashTilePollUpdate(c, r, 0, iteration);
                    if (poll_success) {
                        if constexpr (tuning::verbose) {
                            auto cc = 0, rr = 0;
                            EXPECT_EQ(cc, c);
                            EXPECT_EQ(rr, r);
                        }
#ifdef PLUGIN_RENDER_CALLBACK_APPROACH
                        _nativeGraphicGetRenderCallback()(index);
#endif
                        count++;
                    }
                    return poll_success;
                });
            if (0 == std::distance(index_range.begin(), remove_iter)) {
                iteration++;
                if constexpr (tuning::verbose) {
                    EXPECT_EQ(std::exchange(count, 0), 9);
                }
                index_range = ref_index_range;
                begin_iter = index_range.begin();
                remove_iter = index_range.end();
            }
        }
        const auto t1 = watch.elapsed();
        using core::literals::operator<<;
        XLOG(INFO) << "-- profile parting line\n"
            << "concurrency " << codec_concurrency << "\n"
            << "iteration " << iteration << "\n"
            << "time " << t1 << "\n"
            << "fps " << iteration / t1.count() << "\n";
        _nativeLibraryRelease();
        EXPECT_GE(iteration, 3714);
    };
};

namespace gallery::test
{
    TEST(Plugin, PollLoopProfile) {
        auto profile_codec_concurrency = plugin_routine("http://localhost:8900/Output/NewYork/5x3/NewYork.mpd");
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

    TEST(Plugin, PollMockLoop) {
        
    }


    TEST(Gallery, Concurrency) {
        unsigned codec = 0, net = 0, executor = 0;
        unity::test::_nativeConcurrencyValue(codec, net, executor);
        EXPECT_EQ(codec, 2);
        EXPECT_EQ(net, 8);
        EXPECT_EQ(executor, 8);
        unity::test::_nativeConfigConcurrency(4, 4);
        unity::test::_nativeConcurrencyValue(codec, net, executor);
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
        EXPECT_TRUE(unity::_nativeLoadEnvConfig());
        _nativeLibraryInitialize();
        const auto str = unity::_nativeDashCreate();
        XLOG(INFO) << str;
        EXPECT_FALSE(std::string_view{str}.empty());
    }
}
