#include "pch.h"
#include "network/dash.manager.h"
#include "multimedia/io.segmentor.h"
#include "gallery/pch.h"
#include "gallery/database.sqlite.h"
#include <folly/MoveWrapper.h>
#include <boost/beast/core/multi_buffer.hpp>
#include <objbase.h>
#include "core/meta/exception_trait.hpp"
#include <range/v3/view/cartesian_product.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/action/remove_if.hpp>
#include <boost/process/environment.hpp>
#include <re2/re2.h>
#include <numeric>
#include <folly/Random.h>
#include <folly/Lazy.h>
#include <range/v3/view/enumerate.hpp>

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
            "http://47.101.209.146:33666/NewYork/6x5/NewYork.mpd");
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

    TEST(ParseLog, NetworkBandwidth) {
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

    const auto random_around = [](int central, int span) {
        return central + span / 2 - folly::to<int>(folly::Random::rand32(span));
    };

    auto random_percent_range = [](int size) {
        size = random_around(size, 2);
        std::set<int> s;
        return s;
    };

    TEST(Statistic, QpDistribution) {
        std::filesystem::path dir = "D:/Media/NewYork/6x5";
        auto entry_of_qp = [=](int qp, auto index) {
            return core::filter_directory_entry(dir, [=](const std::filesystem::directory_entry& e) {
                return RE2::PartialMatch(e.path().filename().string(),
                                         fmt::format("_qp{}_dash{}.", qp, index));
            });
        };
        std::map<int, std::map<int, int>> qp_size;
        auto qp_list = { 22, 27, 32, 37, 42 };
        auto qp_func = [&](auto index) {
            return [&](int qp) {
                auto entries = entry_of_qp(qp, index);
                qp_size[qp].push_back(
                    std::accumulate(entries.begin(), entries.end(), 0,
                                    [](int s, std::filesystem::path& p) {
                                        return s + file_size(p);
                                    }));
            };
        };
        auto i = 0;
        for (const std::filesystem::path& p : std::filesystem::directory_iterator{ dir }) {
            auto qp = 0;
            std::string index;
            const auto filename = p.filename().string();
            if (!RE2::PartialMatch(filename, R"(_qp(\d{2})_dash(\w+)\.)", &qp, &index)) continue;
            i++;
            auto pos = index == "init" ? 0 : folly::to<int>(index);
            qp_size[qp][pos] += file_size(p);
        }
        auto row = folly::to<int>(qp_size.begin()->second.size());
        fmt::print("row {}\n", row);
        std::ofstream of{ "F:/GWorkSet/storage.csv", std::ios::trunc };
        std::ofstream of_segment{ "F:/GWorkSet/storage.segment.csv", std::ios::trunc };
        ASSERT_TRUE(of.is_open());
        fmt::print(of, "qp22,qp27,qp32,qp37,qp42\n");
        fmt::print(of_segment, "qp,tile_size\n");
        try {
            for (auto r : ranges::view::ints(0, row)) {
                auto last = 17;
                for (auto& [qp, sz] : qp_size) {
                    EXPECT_EQ(qp, last + 5);
                    last = qp;
                    auto delim = qp < 42 ? ',' : '\n';
                    fmt::print(of_segment, "{},{}\n", qp, sz.at(r) / 1024);
                    if (r) {
                        fmt::print(of, "{}{}", sz.at(r), delim);
                    }
                }
            }
        } catch (std::out_of_range&) {
            FAIL();
        }
        fmt::print("entry {}\n", i);
    }

    TEST(ParseLog, SegmentBitrate) {
        static struct policy
        {
            bool constant_bandwidth = true;
        } policy;
        std::random_device rd;
        std::mt19937 gen{ rd() };
        std::normal_distribution<> d1{ 10000, 500 };
        std::normal_distribution<> d3{ 30000, 2000 };
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
        std::string line;
        std::map<int, std::vector<int>> session_io;
        while (std::getline(reader, line)) {
            auto io_index = 0;
            auto bytes = 0;
            if (!RE2::PartialMatch(line, R"(response=\w+:index=(\d+):transfer=(\d+))", &io_index, &bytes)) continue;
            std::string time;
            auto session_index = 0;
            EXPECT_TRUE(RE2::PartialMatch(line, "\\[(.+)] \\[session\\$(\\d+)]", &time, &session_index));
            auto& record = session_io[session_index];
            record.push_back(bytes);
        }
        auto index = 0;
        std::vector<int> chunk_list;
        try {
            while (++index) {
                chunk_list.push_back(0);
                for (auto& [id, transfer_list] : session_io) {
                    if (transfer_list.size() <= 1) continue;
                    chunk_list.back() += transfer_list.at(index);
                }
            }
        } catch (std::out_of_range&) {
            chunk_list.pop_back();
            fmt::print("total {} MB\n", std::accumulate(
                           chunk_list.begin(), chunk_list.end(), 0ui64) / 1024 / 1024);
        }
        auto max_bandwidth = folly::lazy([&] {
            std::vector<std::pair<int, int>> selection(150, { 0, 0 });
            struct control_block
            {
                int latency = folly::Random::rand64(7);
                int window = 40;
                int half = 20;
            } cb;
            for (auto&& [time, bandwidth] : selection | ranges::view::enumerate) {
                auto offset = time % cb.window;
                if (!policy.constant_bandwidth && offset < cb.half) {
                    bandwidth.first = d1(gen);
                    bandwidth.second = 10'000;
                    continue;
                }
                bandwidth.first = d3(gen);
                while (bandwidth.first > 32'500) {
                    bandwidth.first = d3(gen);
                }
                bandwidth.second = 30'000;
            }
            return selection;
        });
        auto& xxx = max_bandwidth();
        std::ofstream csv{ analyze };
        ASSERT_TRUE(csv.is_open());
        auto play_time = 0;
        fmt::print(csv, "time,bandwidth,method,max\n");
        for (auto&& [time, chunk_size] : chunk_list | ranges::view::enumerate) {
            auto max_size = chunk_size * 8 / 1000;
            auto bandwidth = max_bandwidth().at(time);
            auto request_size = std::min(max_size, bandwidth.first);
            fmt::print(csv, "{},{},ours,{}\n", play_time, request_size, max_size);
            fmt::print(csv, "{},{},bandwidth,{}\n", play_time, bandwidth.second, "null");
            play_time++;
        }
    }

    TEST(EditCsv, Comparison) {
        // vary 20190226.152350 constant 20190227.222903
        std::filesystem::path p = "F:/GWorkSet/analyze.20190226.152350.csv";
        std::ifstream fin{ p };
        ASSERT_TRUE(fin.is_open());
        std::ofstream fout{ p.replace_extension(".edit.csv"), std::ios::trunc };
        ASSERT_TRUE(fout.is_open());
        std::string line;
        if (std::getline(fin, line)) {
            fmt::print(fout, "{},bandwidth.ref\n", line);
        }
        std::random_device rd;
        std::mt19937 gen{ rd() };
        std::normal_distribution<> d1{ 0, 500 };
        std::normal_distribution<> d3{ -500, 1200 };
        auto last_max_size = 0;
        while (std::getline(fin, line)) {
            std::string line_bandwidth;
            EXPECT_TRUE(std::getline(fin, line_bandwidth));
            auto size = 0, max_size = 0, max_bandwidth = 0;
            {
                std::vector<std::string> ours, bandwidth;
                folly::split(',', line, ours);
                folly::split(',', line_bandwidth, bandwidth);
                EXPECT_EQ(line[0], line_bandwidth[0]);
                size = folly::to<int>(ours[1]);
                max_size = folly::to<int>(ours[3]);
                max_bandwidth = folly::to<int>(bandwidth[1]);
            }
            auto next_size = -1;
            while (next_size <= 0 || next_size >= max_size + 1000) {
                if (size < 1000) {
                    next_size = size;
                    break;
                }
                auto dd = size > 22000 ? d3 : d1;
                if (!last_max_size) {
                    last_max_size = size;
                }
                next_size = (size + last_max_size) / 2 + dd(gen);
                last_max_size = next_size;
            }
            fmt::print(fout, "{},{}\n", line, next_size);
            fmt::print(fout, "{},null\n", line_bandwidth);
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
        EXPECT_TRUE(unity::_nativeLibraryConfigLoad(nullptr));
        _nativeLibraryInitialize();
        const auto str = _nativeDashCreate();
        XLOG(INFO) << str;
        EXPECT_FALSE(std::string_view{str}.empty());
    }
}
