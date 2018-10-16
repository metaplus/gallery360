#include "pch.h"
#include "core/pch.h"
#include "network/component.h"
#include "multimedia/component.h"
#include <filesystem>
#include <fstream>
#include <folly/concurrency/UnboundedQueue.h>
#include <folly/executors/ThreadedExecutor.h>
#include <folly/futures/Future.h>
#include <folly/futures/FutureSplitter.h>
#include <folly/stop_watch.h>
#define STATIC_LIBRARY
#include "unity/gallery/pch.h"
#include <boost/beast/core/ostream.hpp>

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

using namespace std::literals;

TEST(Future, DeferError) {
    static_assert(std::is_base_of<std::exception, core::bad_request_error>::value);
    static_assert(std::is_base_of<std::exception, core::not_implemented_error>::value);
    auto sf = folly::makeSemiFutureWith([] { throw core::bad_request_error{ "123" }; return 1; });
    EXPECT_TRUE(sf.hasException());
    EXPECT_FALSE(sf.hasValue());
    EXPECT_THROW(sf.value(), core::bad_request_error);
    sf = std::move(sf).deferError([](auto&) { throw core::not_implemented_error{ "123" }; return 2; });
    EXPECT_THROW(sf.value(), folly::FutureNotReady);
    EXPECT_THROW(std::move(sf).get(), core::not_implemented_error);
    auto[p1, sf1] = folly::makePromiseContract<int>();
    p1.setException(core::bad_request_error{ "123" });
    EXPECT_TRUE(sf1.hasException());
    sf1 = std::move(sf1).deferError([](auto&) { return 2; });
    EXPECT_THROW(sf1.value(), folly::FutureNotReady);
    EXPECT_EQ(std::move(sf1).get(), 2);
}

folly::Future<bool> async_consume(frame_segmentor& segmentor,
                                  pixel_consume& consume,
                                  bool copy = false) {
    if (copy) {
        return folly::async([segmentor, &consume]() mutable { return segmentor.try_consume_once(consume); });
    }
    return folly::async([&segmentor, &consume] { return segmentor.try_consume_once(consume); });
}

frame_indexed_builder create_frame_builder(pixel_consume& consume,
                                           unsigned concurrency = std::thread::hardware_concurrency()) {
    return [&consume, concurrency](auto ordinal,
                                   multi_buffer& head_buffer,
                                   multi_buffer&& tail_buffer)
        -> frame_consumer {
        auto segmentor = folly::makeMoveWrapper(
            frame_segmentor{ core::split_buffer_sequence(head_buffer,tail_buffer),concurrency });
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

TEST(DashManager, StreamTile) {
    core::set_cpu_executor(3);
    auto manager = dash_manager::async_create_parsed("http://localhost:8900/dash/tos_srd_4K.mpd").get();
    EXPECT_EQ(manager.scale_size(), std::make_pair(3840, 1728));
    EXPECT_EQ(manager.grid_size(), std::make_pair(3, 3));
    auto count = 0;
    pixel_consume consume = [&count](pixel_array) { count++; };
    manager.register_represent_builder(create_frame_builder(consume));
    while (manager.available()) {
        if (!manager.poll_tile_consumed(0, 0)) {
            if (!manager.wait_tile_consumed(0, 0)) {
                EXPECT_FALSE(manager.available());
            }
        }
    }
    EXPECT_EQ(count, 250);
}

folly::Future<int> loop_tile_consume(unsigned concurrency,
                                     folly::Executor& executor,
                                     std::string path = "http://localhost:8900/dash/full/tos_srd_4K.mpd"s) {
    return dash_manager::async_create_parsed(path)
        .then(std::addressof(executor),
              [concurrency](dash_manager manager) {
                  EXPECT_EQ(manager.scale_size(), std::make_pair(3840, 1728));
                  EXPECT_EQ(manager.grid_size(), std::make_pair(3, 3));
                  auto count = 0i64;
                  pixel_consume consume = [&count](pixel_array) { count++; };
                  manager.register_represent_builder(create_frame_builder(consume, concurrency));
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
    t0 = profile_by_concurrency(1);
    t1 = profile_by_concurrency(1);     //59s
    t2 = profile_by_concurrency(2);     //42s
    t3 = profile_by_concurrency(3);     //38s   
    t4 = profile_by_concurrency(4);     //37s
    t5 = profile_by_concurrency(8);     //38s
    fmt::print("t0:{}\n,t1:{}\n,t2:{}\n,t3:{}\n,t4:{}\n,t5:{}\n", t0, t1, t2, t3, t4, t5);
}

folly::Future<int> loop_frame_consume(unsigned concurrency,
                                      folly::Executor& executor,
                                      std::string path = "http://localhost:8900/dash/full/tos_srd_4K.mpd"s) {
    return dash_manager::async_create_parsed(path)
        .then(std::addressof(executor),
              [concurrency](dash_manager manager) {
                  EXPECT_EQ(manager.scale_size(), std::make_pair(3840, 1728));
                  EXPECT_EQ(manager.grid_size(), std::make_pair(3, 3));
                  auto count = 0i64;
                  pixel_consume consume = [&count](pixel_array) { count++; };
                  manager.register_represent_builder(create_frame_builder(consume, 8));
                  auto wait_tile_consume = [&manager](int col, int row) {
                      return  [&manager, col, row]() {
                          if (!manager.wait_tile_consumed(col, row)) {
                              EXPECT_FALSE(manager.available());
                          }
                      };
                  };
                  auto[col, row] = manager.grid_size();
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

TEST(DashManager, StreamFrame) {
    core::set_cpu_executor(8);
    auto manager = dash_manager::async_create_parsed("http://localhost:8900/dash/full/tos_srd_4K.mpd").get();
    EXPECT_EQ(manager.scale_size(), std::make_pair(3840, 1728));
    EXPECT_EQ(manager.grid_size(), std::make_pair(3, 3));
    auto count = 0;
    pixel_consume consume = [&count](pixel_array) { count++; };
    manager.register_represent_builder(create_frame_builder(consume, 8));
    auto wait_tile_consume = [&manager](int col, int row) {
        return  [&manager, col, row]() {
            if (!manager.wait_tile_consumed(col, row)) {
                EXPECT_FALSE(manager.available());
            }
        };
    };
    auto[col, row] = manager.grid_size();
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
    t1 = profile_by_concurrency(1);     //6.15min
    t2 = profile_by_concurrency(2);     //6.26min
    t3 = profile_by_concurrency(3);     //6.27min  
    t4 = profile_by_concurrency(4);     //6.15min
    t5 = profile_by_concurrency(8);     //6.15min
    fmt::print("t1:{}\n,t2:{}\n,t3:{}\n,t4:{}\n,t5:{}\n", t1, t2, t3, t4, t5);
}

TEST(String, Insert) {
    std::string x = "";
    x.insert(x.begin(), '/');
    EXPECT_STREQ(x.c_str(), "/");
    x = "123";
    x.insert(x.begin(), '/');
    EXPECT_STREQ(x.c_str(), "/123");
}

TEST(String, Replace) {
    auto replace = [](std::string& s) {
        auto suffix = "123";
        if (s.empty()) {
            return s.assign(fmt::format("/{}", suffix));
        }
        assert(s.front() == '/');
        return s.replace(s.rfind('/') + 1, s.size(), suffix);
    };
    std::string x = "/dash/init.mp4";
    EXPECT_STREQ(replace(x).c_str(), "/dash/123");
    x = "/init.mp4";
    EXPECT_STREQ(replace(x).c_str(), "/123");
    x.clear();
    EXPECT_STREQ(replace(x).c_str(), "/123");
}

TEST(Variant, GetIf) {
    std::variant<std::monostate, int> v;
    EXPECT_FALSE(v.valueless_by_exception());
    EXPECT_EQ(v.index(), 0);
    EXPECT_TRUE(std::get_if<std::monostate>(&v) != nullptr);
    EXPECT_TRUE(std::get_if<int>(&v) == nullptr);
    EXPECT_TRUE(&std::get<std::monostate>(v) != nullptr);
    EXPECT_TRUE(&std::get<std::monostate>(v) == std::get_if<std::monostate>(&v));
}

using namespace unity;


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
        //EXPECT_EQ(pending.size() + poll_count, 9);
        for (auto& task : pending) {
            task();
        }
        iteration++;
    }
    render_event_func(-1);
    auto t1 = watch.elapsed();
    fmt::print("t1:{}\n", t1);
    EXPECT_EQ(iteration, 3715);
}

auto loop_poll_frame = []() {
    folly::stop_watch<seconds> watch;
    _nativeConfigExecutor();
    _nativeDashCreate("http://localhost:8900/dash/NewYork/5k/NewYork_5k.mpd");
    int col = 0, row = 0, width = 0, height = 0;
    EXPECT_TRUE(_nativeDashGraphicInfo(col, row, width, height));
    EXPECT_EQ(col, 3);
    EXPECT_EQ(row, 3);
    EXPECT_EQ(width, 3840);
    EXPECT_EQ(height, 1920);
    _nativeDashPrefetch();
    auto iteration = 0;
    std::vector<int> ref_index_range;
    for (auto r = 0; r < row; ++r) {
        for (auto c = 0; c < col; ++c) {
            ref_index_range.push_back(r * col + c + 1);
            _nativeDashSetTexture(c, r, nullptr, nullptr, nullptr);
        }
    }
    debug::_nativeMockGraphic();
    auto index_range = ref_index_range;
    auto remove_iter = index_range.end();
    auto count = 0;
    while (_nativeDashAvailable()) {
        remove_iter = std::remove_if(
            index_range.begin(), remove_iter,
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
            EXPECT_EQ(std::exchange(count, 0), 9);
            index_range = ref_index_range;
            remove_iter = index_range.end();
        }
    }
    const auto t1 = watch.elapsed();
    fmt::print("time:{} fps:{}\n", t1, iteration / t1.count());
    EXPECT_EQ(iteration, 3714);
    return t1;
};

TEST(Galley, PluginPoll) {
    loop_poll_frame();  // fps 53

}

TEST(Executor, ThreadedExecutor) {
    uint64_t i1 = 0, i2 = 0, i3 = 0;
    auto i0 = folly::getCurrentThreadID();
    folly::stop_watch<milliseconds> watch;
    {
        auto executor = core::make_threaded_executor("Test");
        watch.reset();
        executor->add([&] { std::this_thread::sleep_for(200ms); i1 = folly::getCurrentThreadID(); });
        executor->add([&] { std::this_thread::sleep_for(150ms); i2 = folly::getCurrentThreadID(); });
        executor->add([&] { std::this_thread::sleep_for(100ms); i3 = folly::getCurrentThreadID(); });
    }
    auto elapsed = watch.elapsed();
    EXPECT_GE(elapsed, 200ms);
    EXPECT_LT(elapsed, 250ms);
    EXPECT_NE(i0, i1);
    EXPECT_NE(i0, i2);
    EXPECT_NE(i0, i3);
    EXPECT_NE(i1, i2);
    EXPECT_NE(i1, i3);
    EXPECT_NE(i2, i3);
}

TEST(Future, PreFullfill) {
    folly::Promise<int> p;
    EXPECT_NO_THROW(p.setValue(1));
    {
        auto f = p.getSemiFuture();
        EXPECT_TRUE(f.hasValue());
        EXPECT_EQ(std::move(f).get(), 1);
    }
    EXPECT_THROW(p.getSemiFuture(), folly::FutureAlreadyRetrieved);
}

TEST(Future, FutureSplitterProfile) {
    {
        folly::FutureSplitter<int> fs{ folly::makeSemiFuture(1).via(&folly::InlineExecutor::instance()) };
        EXPECT_TRUE(fs.getSemiFuture().hasValue());
        EXPECT_TRUE(fs.getFuture().hasValue());
    }
    {
        using std::chrono::microseconds;
        multi_buffer buffer;
        boost::beast::ostream(buffer) << std::ifstream{ "F:/Gpac/debug/NewYork/5k/segment_2_2_5k_init.mp4",std::ios::binary }.rdbuf();
        EXPECT_EQ(buffer.size(), 925);                      // ~ 1kB
        folly::FutureSplitter<multi_buffer> fs{ folly::makeFuture(buffer) };
        folly::FutureSplitter<std::shared_ptr<multi_buffer>> fs2{ folly::makeFuture(std::make_shared<multi_buffer>(buffer)) };
        std::vector<microseconds> time_trace(20);
        {
            folly::stop_watch<microseconds> watch;
            const void* last = nullptr;
            for (auto& t : time_trace) {
                {
                    watch.reset();
                    auto fss = fs.getSemiFuture();
                    t = watch.elapsed();                    // ~ 10us
                    auto* data = (*fss.value().data().begin()).data();
                    auto fss2 = fs.getSemiFuture();
                    auto* data2 = (*fss2.value().data().begin()).data();
                    EXPECT_EQ(fss.value().size(), 925);
                    EXPECT_NE(data, data2);
                    if (std::exchange(last, data)) {
                        EXPECT_EQ(data, last);
                        EXPECT_NE(data, (*buffer.data().begin()).data());
                    }
                    fmt::print("elapsed: {}\n", t);
                }
                {
                    watch.reset();
                    auto fss = fs2.getSemiFuture();
                    auto t2 = watch.elapsed();              // ~ 2us
                    EXPECT_GT(t, t2);
                    fmt::print("elapsed-ptr: {}\n", t2);
                }
            }
        }
    }
}

TEST(Container, UnboundedQueue) {
    {
        folly::USPSCQueue<int, true> q;
        EXPECT_TRUE(q.empty());
        EXPECT_TRUE(q.try_peek() == nullptr);
        EXPECT_FALSE(q.try_dequeue_for(50ms).hasValue());
        q.enqueue(1);
        q.enqueue(2);
        q.enqueue(3);
        EXPECT_EQ(*q.try_peek(), 1);
        auto x = 0;
        q.dequeue(x);
        EXPECT_EQ(x, 1);
    }
    {
        std::vector<int> v{ 1,2,3,4,5 };
        auto* p = v.data();
        folly::USPSCQueue<std::vector<int>, true> q;
        q.enqueue(std::move(v));
        EXPECT_TRUE(v.empty());
        EXPECT_EQ(q.size(), 1);
        q.dequeue(v);
        EXPECT_EQ(v.data(), p);
        EXPECT_EQ(v.size(), 5);
        EXPECT_TRUE(q.empty());
        q.enqueue(std::move(v));
        EXPECT_EQ(q.size(), 1);
        decltype(v) v2;
        EXPECT_TRUE(q.try_dequeue(v2));
        EXPECT_TRUE(q.empty());
        EXPECT_EQ(v2.data(), p);
    }
}

TEST(Container, MPMCQueue) {
    folly::MPMCQueue<int> q{ 2 };
    EXPECT_EQ(q.capacity(), 2);
    EXPECT_TRUE(q.isEmpty());
    folly::Baton<> b;
    decltype(steady_clock::now()) t1, t2, t3, t4;
    std::thread{ [&] {
        q.blockingWrite(1);
        EXPECT_TRUE(q.write(2));
        EXPECT_FALSE(q.writeIfNotFull(3));
        EXPECT_TRUE(q.isFull());
        t1 = steady_clock::now();
        b.post();
        std::this_thread::sleep_for(100ms);
        q.blockingWrite(3);
    } }.detach();
    b.wait();
    t2 = steady_clock::now();
    int i;
    EXPECT_TRUE(q.read(i));
    EXPECT_EQ(i, 1);
    EXPECT_TRUE(q.readIfNotEmpty(i));
    EXPECT_EQ(i, 2);
    t3 = steady_clock::now();
    q.blockingRead(i);
    t4 = steady_clock::now();
    EXPECT_EQ(i, 3);
    EXPECT_LT(t1, t2);
    EXPECT_GT(t4 - t3, 90ms);
}

TEST(FileSystem, CurrentPath) {
    auto p = std::filesystem::current_path();
    EXPECT_TRUE(std::filesystem::is_directory(p));
    EXPECT_STREQ(p.generic_string().data(), "D:/Project/gallery360/test/gallery-test");
    auto fp = std::filesystem::current_path() / "test.cpp";
    EXPECT_STREQ(fp.generic_string().data(), "D:/Project/gallery360/test/gallery-test/test.cpp");
    auto fp2 = std::filesystem::current_path().parent_path() / "test.cpp";
    EXPECT_STREQ(fp2.generic_string().data(), "D:/Project/gallery360/test/test.cpp");
    EXPECT_TRUE(std::filesystem::is_regular_file(fp));
    std::ifstream fin{ fp, std::ios::binary };
    EXPECT_TRUE(fin.is_open());
    std::string str{ std::istreambuf_iterator<char>{fin},std::istreambuf_iterator<char>{} };
    EXPECT_EQ(std::size(str), std::filesystem::file_size(fp));
}

TEST(Executor, MakePool) {
    auto executor = core::make_pool_executor(10);
    EXPECT_EQ(executor->numActiveThreads(), 1);
    EXPECT_EQ(executor->numThreads(), 10);
    EXPECT_EQ(executor->getPendingTaskCount(), 0);
    EXPECT_STREQ(executor->getName().data(), "CorePool");
    EXPECT_EQ(executor->getPoolStats().idleThreadCount, 10);
    EXPECT_EQ(executor->getPoolStats().activeThreadCount, 0);
}

TEST(Wrapper, Try) {
    {
        folly::Try<int> t;
        EXPECT_FALSE(t.hasValue());
        EXPECT_FALSE(t.hasException());
        t.emplace(1);
        EXPECT_TRUE(t.hasValue());
        EXPECT_EQ(t.value(), 1);
        t.emplaceException(std::logic_error{ "123" });
        EXPECT_TRUE(t.hasException());
        EXPECT_TRUE(t.exception().is_compatible_with<std::exception>());
        EXPECT_STREQ(t.exception().class_name().c_str(), "class std::logic_error");
        EXPECT_THROW(t.throwIfFailed(), std::exception);
        EXPECT_THROW(t.throwIfFailed(), std::logic_error);
        EXPECT_NO_THROW(dynamic_cast<std::logic_error&>(*t.tryGetExceptionObject()));
        t.emplace();
        EXPECT_TRUE(t.hasValue());
        EXPECT_EQ(t.value(), 0);
    }
    {
        folly::Try<std::unique_ptr<int>> t;
        EXPECT_FALSE(t.hasValue());
        t.emplace(std::make_unique<int>(2));
        auto t2 = std::move(t);
        EXPECT_TRUE(t.hasValue());              //! default constructed state after moved out, not empty
        EXPECT_FALSE(t.value() != nullptr);
        EXPECT_TRUE(t2.hasValue());
        EXPECT_EQ(*t2.value(), 2);
    }
}

