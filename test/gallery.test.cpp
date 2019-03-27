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
#include <boost/process/environment.hpp>
#include <re2/re2.h>
#include <numeric>

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
            update_time += 11ms;
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
                if (++counters.frame % 100 == 0 || counters.frame > 3714) {
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

    struct log_cache_slot
    {
        system_clock::time_point request_time;
        size_t request_size = 0;
        size_t io_index = 0;
        size_t session_index = 0;
    };

    struct statistic
    {
        std::map<double, double> data;
        double interval = 0;

        static constexpr double mega_scale = 1'000'000;

        statistic(double begin, double end, double interval)
            : interval{ interval } {
            for (auto iter = begin; iter <= end; iter += interval) {
                data[iter] = 0;
            }
        }

        void add_record(double begin, double end, double sum) {
            std::optional<double> time_begin, time_end;
            for (auto& [time,count] : data) {
                if (!time_begin.has_value()) {
                    if (time > begin) {
                        if (time - interval >= data.begin()->first) time_begin = time - interval;
                        else time_begin = time;
                    } else if (time == begin) time_begin = time;
                } else if (!time_end.has_value()) {
                    if (time >= end) time_end = time;
                } else break;
            }
            EXPECT_LT(time_begin, time_end);
            const auto average = sum * interval / (*time_end - *time_begin);
            for (auto iter = *time_begin; iter < *time_end; iter += interval) {
                try {
                    data.at(iter) += average;
                } catch (...) {
                    FAIL();
                }
            }
        }

        void remove_record_after(double pos) {
            while (!data.empty()) {
                auto largest = data.rbegin();
                if (largest->first <= pos) return;
                data.erase(largest.base());
            }
        }

        double total(double scale = mega_scale) {
            double total_count = 0;
            for (auto& [time, count] : data) {
                total_count += count;
            }
            return total_count / scale;
        }
    };

    TEST(ParseLog, NetworkEvent) {
        const auto workset = boost::this_process::environment()["GWorkSet"].to_string();
        const auto analyze = std::filesystem::path{ workset }
            / absl::StrJoin({ "analyze"s, core::local_date_time("%Y%m%d.%H%M%S"), "csv"s }, ".");
        create_directories(analyze.parent_path());
        ASSERT_FALSE(workset.empty());
        const auto trace_test = core::last_write_path_of_directory(workset);
        ASSERT_TRUE(is_directory(trace_test));
        const auto network_log = trace_test / "network.log";
        ASSERT_TRUE(is_regular_file(network_log));
        std::ifstream reader{ network_log };
        ASSERT_TRUE(reader.is_open());
        statistic statistic{ 0, 120, 0.25 };
        std::string line;
        size_t total = 0;
        std::map<int, log_cache_slot> session_io;
        std::optional<system_clock::time_point> start_time;
        auto start_time_offset = [&start_time](system_clock::time_point sys_time) {
            using namespace std::chrono;
            return duration_cast<duration<double>>(sys_time - *start_time).count();
        };
        while (std::getline(reader, line)) {
            size_t io_index = 0;
            size_t bytes = 0;
            if (!RE2::PartialMatch(line, ":index=(\\d+):transfer=(\\d+)", &io_index, &bytes)) continue;
            total += bytes;
            std::string time;
            auto session_index = 0;
            EXPECT_TRUE(RE2::PartialMatch(line, "\\[(.+)] \\[session\\$(\\d+)]", &time, &session_index));
            absl::Time t;
            std::string err;
            EXPECT_TRUE(absl::ParseTime("%Y-%m-%d %H:%M:%S.%E*f", time, &t, &err));
            const auto sys_time = ToChronoTime(t);
            if (!start_time.has_value()) {
                start_time.emplace(sys_time);
            }
            auto& log_slot = session_io[session_index];
            if (const auto is_request = RE2::PartialMatch(line, "request="); is_request) {
                EXPECT_EQ(log_slot.request_size, 0);
                log_slot.request_time = sys_time;
                log_slot.request_size = bytes;
                log_slot.session_index = session_index;
                log_slot.io_index = io_index;
            } else {
                EXPECT_NE(log_slot.request_size, 0);
                EXPECT_EQ(log_slot.io_index, io_index);
                log_slot.request_size = 0;
                auto request_time = start_time_offset(log_slot.request_time);
                auto response_time = start_time_offset(sys_time);
                auto io_size = bytes + log_slot.request_size;
                statistic.add_record(request_time, response_time,
                                     folly::to<double>(io_size) * 8);
            }
        }
        fmt::print("total {} MB\n", statistic.total(8 * 1024 * 1024));
        std::ofstream csv{ analyze };
        ASSERT_TRUE(csv.is_open());
        double window = 1, time_temp = window, count_temp = 0;
        fmt::print(csv, "{},{}\n", "time", "bandwidth");
        fmt::print(csv, "{},{}\n", 0, 0);
        for (auto& [time, count] : statistic.data) {
            count_temp += count;
            if (time >= time_temp) {
                time_temp += window;
                fmt::print(csv, "{},{}\n", time_temp, count_temp / statistic::mega_scale);
                count_temp = 0;
            }
        }
    }

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
