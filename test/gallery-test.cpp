#include "pch.h"
#include "core/pch.h"
#include "network/component.h"
#include "multimedia/component.h"
#include <folly/stop_watch.h>

//#define STATIC_LIBRARY
#include "unity/gallery/pch.h"

using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::steady_clock;
using boost::asio::const_buffer;
using boost::beast::multi_buffer;
using net::component::dash_manager;
using net::component::frame_consumer;
using net::component::frame_builder;
using net::component::frame_indexed_builder;
using net::component::ordinal;
using media::component::frame_segmentor;
using media::component::pixel_array;
using media::component::pixel_consume;

using namespace unity;

namespace debug
{
    constexpr auto verbose = false;
    constexpr auto trace = false;
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

folly::Future<int> loop_tile_consume(unsigned concurrency,
                                     folly::Executor& executor,
                                     std::string path = "http://localhost:8900/dash/full/tos_srd_4K.mpd"s) {
    return dash_manager::create_parsed(path)
        .then(std::addressof(executor),
              [concurrency](dash_manager manager) {
                  EXPECT_EQ(manager.scale_size(), std::make_pair(3840, 1728));
                  EXPECT_EQ(manager.grid_size(), std::make_pair(3, 3));
                  auto count = 0i64;
                  pixel_consume consume = [&count](pixel_array) {
                      count++;
                  };
                  //manager.register_represent_builder(create_frame_builder(consume, concurrency));
                  while (manager.available()) {
                      if (!manager.poll_tile_consumed(0, 0)) {
                          if (!manager.wait_tile_consumed(0, 0)) {
                              EXPECT_FALSE(manager.available());
                          }
                      }
                  }
                  return count;
              });
}

namespace gallery_test
{
    TEST(DashManager, StreamTile) {
        core::set_cpu_executor(3);
        auto manager = dash_manager::create_parsed("http://localhost:8900/dash/tos_srd_4K.mpd").get();
        EXPECT_EQ(manager.scale_size(), std::make_pair(3840, 1728));
        EXPECT_EQ(manager.grid_size(), std::make_pair(3, 3));
        auto count = 0;
        pixel_consume consume = [&count](pixel_array) {
            count++;
        };
        //manager.register_represent_builder(create_frame_builder(consume));
        while (manager.available()) {
            if (!manager.poll_tile_consumed(0, 0)) {
                if (!manager.wait_tile_consumed(0, 0)) {
                    EXPECT_FALSE(manager.available());
                }
            }
        }
        EXPECT_EQ(count, 250);
    }

    TEST(DashManager, StreamTileProfile) {
        core::set_cpu_executor(4);
        auto executor = folly::getCPUExecutor();
        seconds t0, t1, t2, t3, t4, t5;
        auto profile_by_concurrency = [executor](unsigned concurrency) {
            folly::stop_watch<seconds> watch;
            auto async_count = loop_tile_consume(concurrency, *executor);
            EXPECT_EQ(std::move(async_count).get(), 17616);
            return watch.elapsed();
        };
        std::cout << (t0 = profile_by_concurrency(1));
        std::cout << (t1 = profile_by_concurrency(1)); //59s
        std::cout << (t2 = profile_by_concurrency(2)); //42s
        std::cout << (t3 = profile_by_concurrency(3)); //38s   
        std::cout << (t4 = profile_by_concurrency(4)); //37s
        std::cout << (t5 = profile_by_concurrency(8)); //38s
    }
}

folly::Future<int> loop_frame_consume(unsigned concurrency,
                                      folly::Executor& executor,
                                      std::string path = "http://localhost:8900/dash/full/tos_srd_4K.mpd"s) {
    return dash_manager::create_parsed(path)
        .then(std::addressof(executor),
              [concurrency](dash_manager manager) {
                  EXPECT_EQ(manager.scale_size(), std::make_pair(3840, 1728));
                  EXPECT_EQ(manager.grid_size(), std::make_pair(3, 3));
                  auto count = 0i64;
                  pixel_consume consume = [&count](pixel_array) {
                      count++;
                  };
                  //manager.register_represent_builder(create_frame_builder(consume, 8));
                  auto wait_tile_consume = [&manager](int col, int row) {
                      return [&manager, col, row]() {
                          if (!manager.wait_tile_consumed(col, row)) {
                              EXPECT_FALSE(manager.available());
                          }
                      };
                  };
                  auto [col, row] = manager.grid_size();
                  auto iteration = 0;
                  while (manager.available()) {
                      std::vector<std::invoke_result_t<decltype(wait_tile_consume), int, int>> pending;
                      auto poll_count = 0;
                      for (auto c = 0; c < col; ++c) {
                          for (auto r = 0; r < row; ++r) {
                              auto index = r * col + c + 1;
                              if (!manager.poll_tile_consumed(c, r)) {
                                  pending.push_back(wait_tile_consume(c, r));
                              } else {
                                  poll_count++;
                              }
                          }
                      }
                      //EXPECT_EQ(pending.size() + poll_count, 9);
                      for (auto& task : pending) {
                          task();
                      }
                      iteration++;
                  }
                  return iteration;
              });
}

namespace gallery_test
{
    TEST(DashManager, StreamFrame) {
        core::set_cpu_executor(8);
        auto manager = dash_manager::create_parsed("http://localhost:8900/dash/full/tos_srd_4K.mpd").get();
        EXPECT_EQ(manager.scale_size(), std::make_pair(3840, 1728));
        EXPECT_EQ(manager.grid_size(), std::make_pair(3, 3));
        auto count = 0;
        pixel_consume consume = [&count](pixel_array) {
            count++;
        };
        //manager.register_represent_builder(create_frame_builder(consume, 8));
        auto wait_tile_consume = [&manager](int col, int row) {
            return [&manager, col, row]() {
                if (!manager.wait_tile_consumed(col, row)) {
                    EXPECT_FALSE(manager.available());
                }
            };
        };
        auto [col, row] = manager.grid_size();
        auto iteration = 0;
        while (manager.available()) {
            std::vector<std::invoke_result_t<decltype(wait_tile_consume), int, int>> pending;
            auto poll_count = 0;
            for (auto r = 0; r < row; ++r) {
                for (auto c = 0; c < col; ++c) {
                    auto index = r * col + c + 1;
                    if (!manager.poll_tile_consumed(c, r)) {
                        pending.push_back(wait_tile_consume(c, r));
                    } else {
                        poll_count++;
                    }
                }
            }
            EXPECT_EQ(pending.size() + poll_count, 9);
            for (auto& task : pending) {
                task();
            }
            iteration++;
        }
        EXPECT_EQ(iteration, 17616);
    }

    TEST(DashManager, StreamFrameProfile) {
        core::set_cpu_executor(8);
        auto executor = folly::getCPUExecutor();
        seconds t1, t2, t3, t4, t5;
        auto profile_by_concurrency = [executor](unsigned concurrency) {
            folly::stop_watch<seconds> watch;
            auto async_count = loop_frame_consume(concurrency, *executor);
            EXPECT_EQ(std::move(async_count).get(), 17616);
            return watch.elapsed();
        };
        std::cout << (t1 = profile_by_concurrency(1)); //6.15min
        std::cout << (t2 = profile_by_concurrency(2)); //6.26min
        std::cout << (t3 = profile_by_concurrency(3)); //6.27min  
        std::cout << (t4 = profile_by_concurrency(4)); //6.15min
        std::cout << (t5 = profile_by_concurrency(8)); //6.15min
    }

    TEST(Galley, Plugin) {
        auto* render_event_func = _nativeGraphicGetRenderEventFunc();
        EXPECT_TRUE(render_event_func != nullptr);
        folly::stop_watch<seconds> watch;
        _nativeConfigExecutor();
        _nativeDashCreate("http://localhost:8900/dash/NewYork/5k/NewYork_5k.mpd");
        int col = 0, row = 0, width = 0, height = 0;
        EXPECT_TRUE(_nativeDashGraphicInfo(col, row, width, height));
        EXPECT_EQ(col, 3);
        EXPECT_EQ(row, 3);
        EXPECT_EQ(width, 3840);
        EXPECT_EQ(height, 1920);
        _nativeGraphicSetTextures(nullptr, nullptr, nullptr);
        _nativeDashPrefetch();
        auto iteration = 0;
        while (_nativeDashAvailable()) {
            std::vector<folly::Function<void()>> pending;
            auto poll_count = 0;
            for (auto r = 0; r < row; ++r) {
                for (auto c = 0; c < col; ++c) {
                    auto index = r * col + c + 1;
                    if (_nativeDashTilePollUpdate(c, r)) {
                        poll_count++;
                        render_event_func(-1);
                    } else {
                        pending.push_back(
                            [c, r, render_event_func, index] {
                                if (_nativeDashTileWaitUpdate(c, r)) {
                                    render_event_func(-1);
                                } else {
                                    EXPECT_FALSE(_nativeDashAvailable());
                                }
                            });
                    }
                }
            }
            if constexpr (debug::verbose) {
                EXPECT_EQ(pending.size() + poll_count, 9);
            }
            for (auto& task : pending) {
                task();
            }
            iteration++;
        }
        render_event_func(-1);
        std::cout << (watch.elapsed());
        EXPECT_EQ(iteration, 3715);
    }
}

auto loop_poll_frame = [](unsigned codec_concurrency = 8) {
    test::_nativeConfigConcurrency(codec_concurrency);
    test::_nativeMockGraphic();
    _nativeConfigExecutor();
    _nativeDashCreate("http://localhost:8900/Output/NewYork/NewYork.mpd");
    int col = 0, row = 0, width = 0, height = 0;
    ASSERT_TRUE(_nativeDashGraphicInfo(col, row, width, height));
    ASSERT_EQ(col, 3);
    ASSERT_EQ(row, 3);
    ASSERT_EQ(width, 3840);
    ASSERT_EQ(height, 1920);
    auto iteration = 0;
    std::vector<int> ref_index_range;
    for (auto r = 0; r < row; ++r) {
        for (auto c = 0; c < col; ++c) {
            ref_index_range.push_back(r * col + c + 1);
            _nativeDashSetTexture(c, r, nullptr, nullptr, nullptr);
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
                const auto r = (index - 1) / col;
                const auto c = (index - 1) % col;
                const auto poll_success = _nativeDashTilePollUpdate(c, r);
                if (poll_success) {
                    _nativeGraphicGetRenderEventFunc()(index);
                    count++;
                }
                return poll_success;
            });
        if (0 == std::distance(index_range.begin(), remove_iter)) {
            iteration++;
            if constexpr (debug::verbose) {
                EXPECT_EQ(std::exchange(count, 0), 9);
            }
            index_range = ref_index_range;
            begin_iter = index_range.begin();
            remove_iter = index_range.end();
        }
    }
    const auto t1 = watch.elapsed();
    fmt::print(std::cerr, "concurrency {} time {} fps {}\n", codec_concurrency, t1.count(), iteration / t1.count());
    _nativeLibraryRelease();
    EXPECT_LT(iteration, 3710);
};

namespace gallery_test
{
    TEST(Galley, PluginPoll) {
        loop_poll_frame(8); // fps 71
        loop_poll_frame(4); // fps 71
        loop_poll_frame(3); // fps 71
        loop_poll_frame(2); // fps 71
        loop_poll_frame(1); // fps 71
    }
}
