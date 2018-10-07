#include "pch.h"
#include <boost/beast/core/multi_buffer.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/logic/tribool.hpp>
#include <fmt/format.h>
#include <folly/executors/Async.h>
#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/executors/GlobalExecutor.h>
#include <folly/executors/task_queue/LifoSemMPMCQueue.h>
#include <folly/executors/task_queue/UnboundedBlockingQueue.h>
#include <folly/Function.h>
#include <folly/futures/Future.h>
#include <folly/futures/FutureSplitter.h>
#include <folly/futures/SharedPromise.h>
#include <folly/MoveWrapper.h>
#include <folly/stop_watch.h>
#include <folly/system/ThreadName.h>
#include <regex>
#include <variant>
#include "network/component.h"

using namespace std::literals;

TEST(Folly, Function) {
    folly::Function<void() const> f;
    EXPECT_TRUE(f == nullptr);
    EXPECT_FALSE(f);
    f = [] {};
    EXPECT_FALSE(f == nullptr);
    EXPECT_TRUE(f);
    f = nullptr;
    EXPECT_TRUE(f == nullptr);
    EXPECT_FALSE(f);
}

TEST(Folly, MoveWrapper) {
    std::string x = "123";
    EXPECT_FALSE(x.empty());
    auto y = folly::makeMoveWrapper(x);
    EXPECT_TRUE(x.empty());
    EXPECT_FALSE(y->empty());
    auto z = [y] {
        EXPECT_FALSE(y->empty());
    };
    EXPECT_TRUE(y->empty());
    z();
}

TEST(Folly, LifoSemMPMCQueue) {
    folly::LifoSemMPMCQueue<int, folly::QueueBehaviorIfFull::THROW> queue{ 1 };
    queue.add(1); folly::Promise<int> p;
    EXPECT_THROW(queue.add(1), std::runtime_error);
}

TEST(Future, Promise) {
    folly::Promise<int> p;
    auto sf = p.getSemiFuture()
        .deferValue([](int x) { return std::to_string(x) + "123"; });
    p.setWith([] { return 456; });
    EXPECT_STREQ(std::move(sf).get().c_str(), "456123");
}

TEST(Future, CollectAllSemiFuture) {
    auto c1 = folly::collectAllSemiFuture(
        folly::makeSemiFuture(1),
        folly::makeSemiFutureWith([] { return 2; })
    );
    auto[r10, r11] = std::move(c1).get();
    EXPECT_EQ(r10.value(), 1);
    EXPECT_EQ((r11.get<false, int>()), 2);
}

TEST(Future, MakeSemiFutureWith) {
    std::chrono::seconds t1, t2;
    auto executor = std::make_shared<folly::CPUThreadPoolExecutor>(3);
    folly::setCPUExecutor(executor);
    folly::stop_watch<std::chrono::seconds> watch;
    auto sf = folly::makeSemiFutureWith(
        [&] {
            std::this_thread::sleep_for(std::chrono::seconds{ 1 });
            t2 = watch.elapsed();
        });
    std::this_thread::sleep_for(std::chrono::seconds{ 1 });
    t1 = watch.elapsed();
    EXPECT_EQ(t1, 2s);
    EXPECT_EQ(t2, 1s);
}

TEST(Future, MakeEmpty) {
    auto sf = folly::SemiFuture<int>::makeEmpty();
    EXPECT_FALSE(sf.valid());
    EXPECT_THROW(sf.isReady(), folly::FutureInvalid);
    auto f = folly::Future<int>::makeEmpty();
    EXPECT_FALSE(sf.valid());
    EXPECT_THROW(sf.isReady(), folly::FutureInvalid);
}

TEST(Future, ViaExecutor) {
    int y = 0;
    auto executor = std::make_shared<folly::CPUThreadPoolExecutor>(3);
    folly::Promise<folly::Unit> p;
    folly::setCPUExecutor(executor);
    std::chrono::seconds t1, t2, t3, t4, t5;
    folly::stop_watch<std::chrono::seconds> watch;
    auto f1 = folly::async(
        [&] {
            std::this_thread::sleep_for(1s);
            t1 = watch.elapsed();
            p.setValue();
            std::this_thread::sleep_for(3s);
        });
    auto f = p.getSemiFuture()
        .deferValue([](auto) { return 2; })
        .via(executor.get())
        .then(
            [&](int x) {
                y = x;
                t2 = watch.elapsed();
                std::this_thread::sleep_for(1s);
            });
    t3 = watch.elapsed();
    f.wait();
    t4 = watch.elapsed();
    f1.wait();
    t5 = watch.elapsed();
    EXPECT_EQ(y, 2);
    EXPECT_EQ(t1, 1s);
    EXPECT_EQ(t2, 1s);
    EXPECT_EQ(t3, 0s);
    EXPECT_EQ(t4, 2s);
    EXPECT_EQ(t5, 4s);
}

template<class... Ts> struct overload : Ts... { using Ts::operator()...; };
template<class... Ts> overload(Ts...)->overload<Ts...>;

#pragma warning(disable:244 267 101)
TEST(Core, Overload) {
    using variant = std::variant<int, long, double, std::string>;
    variant v{ 1 };
    overload overload{
        [](auto arg) -> int { return arg * 2; },
        [](double arg) -> int { return arg + 0.1; },
        [](const std::string& arg) -> int { return arg.size(); } };
    auto r1 = std::visit(overload, variant{ 1 });
    EXPECT_EQ(r1, 2);
    auto r2 = std::visit(overload, variant{ 1.2 });
    EXPECT_EQ(r2, 1);
    auto r3 = std::visit(overload, variant{ std::string{"123"} });
    EXPECT_EQ(r3, std::string{ "123" }.size());
}

void set_cpu_executor(int concurrency, int queue_size, std::string_view pool_name = "CorePool") {
    static auto executor = std::make_shared<folly::CPUThreadPoolExecutor>(
        std::make_pair(concurrency, 1),
        std::make_unique<folly::LifoSemMPMCQueue<folly::CPUThreadPoolExecutor::CPUTask, folly::QueueBehaviorIfFull::BLOCK>>(queue_size),
        std::make_shared<folly::NamedThreadFactory>(pool_name.data()));
    folly::setCPUExecutor(executor);
}

void set_cpu_executor(int concurrency, std::string_view pool_name = "CorePool") {
    static auto executor = std::make_shared<folly::CPUThreadPoolExecutor>(
        std::make_pair(concurrency, 2),
        std::make_unique<folly::UnboundedBlockingQueue<folly::CPUThreadPoolExecutor::CPUTask>>(),
        std::make_shared<folly::NamedThreadFactory>(pool_name.data()));
    folly::setCPUExecutor(executor);
};

TEST(Executor, SetCpuExecutor) {
    set_cpu_executor(std::thread::hardware_concurrency());
    folly::Synchronized<std::set<uint64_t>> ids;
    auto loop = 8192;
    auto executor = folly::getCPUExecutor();
    while (--loop) {
        folly::via(executor.get(),
                   [&ids] {
                       ids.withWLock(
                           [](std::set<uint64_t>& set) {
                               set.emplace(folly::getCurrentThreadID());
                           });
                   });
    }
    EXPECT_EQ(ids->size(), 8);
}

TEST(Executor, QueuedImmediateExecutor) {
    uint64_t id0 = 0, id1 = 0, id2 = 0, id3 = 0;
    folly::QueuedImmediateExecutor executor;
    set_cpu_executor(1);
    {
        std::chrono::milliseconds t1, t2, t3, t4, t5;
        folly::stop_watch<std::chrono::milliseconds> watch;
        executor.add(
            [&] {
                std::this_thread::sleep_for(5ms);
                t1 = watch.elapsed();
                executor.add(
                    [&] {
                        t3 = watch.elapsed();
                        std::this_thread::sleep_for(5ms);
                    });
                std::this_thread::sleep_for(5ms);
                t2 = watch.elapsed();
                std::this_thread::sleep_for(5ms);
            });
        t4 = watch.elapsed();
        executor.add(
            [&] {
                std::this_thread::sleep_for(5ms);
                t5 = watch.elapsed();
            });
        EXPECT_LT(t1, t2);
        EXPECT_LT(t2, t3);
        EXPECT_LT(t3, t4);
        EXPECT_LT(t4, t5);
    }
    {
        std::chrono::microseconds t1, t2, t3;
        folly::stop_watch<std::chrono::microseconds> watch;
        id0 = folly::getCurrentThreadID();
        folly::SemiFuture<folly::Unit> f = folly::async(
            [&] {
                id1 = folly::getCurrentThreadID();
                return 1;
            }
        ).then(
            &executor,
            [&](folly::Try<int> i) {
                id2 = folly::getCurrentThreadID();
                t2 = watch.elapsed();
            }
        ).semi().deferValue(
            [&](folly::Unit u) {
                id3 = folly::getCurrentThreadID();
                t3 = watch.elapsed();
            });
            std::this_thread::sleep_for(300ms);
            t1 = watch.elapsed();
            EXPECT_EQ(id1, id2);
            EXPECT_GT(t1, t2);
            EXPECT_EQ(id3, 0);
            f.wait();
            EXPECT_EQ(id3, id0);
            EXPECT_GT(t3, t1);
    }
}

auto config_executor = [](auto concurrency) {
    set_cpu_executor(concurrency);
    auto* executor = folly::getCPUExecutor().get();
    EXPECT_NO_THROW(dynamic_cast<folly::CPUThreadPoolExecutor&>(*executor));
    return executor;
};

TEST(Future, DeferValue) {
    auto* executor = config_executor(1);
    auto[p, sf] = folly::makePromiseContract<int>();
    auto id0 = folly::getCurrentThreadID();
    auto id1 = 0;
    auto id2 = 0;
    executor->add(
        [&] {
            std::this_thread::sleep_for(1s);
            id1 = folly::getCurrentThreadID();
            p.setValue(1);
        });
    std::this_thread::sleep_for(2s);
    sf = std::move(sf).deferValue(
        [&](int i) {
            id2 = folly::getCurrentThreadID();
            return i + 1;
        });
    EXPECT_EQ(std::move(sf).get(), 2);
    EXPECT_EQ(id0, id2);
    EXPECT_NE(id0, id1);
}

TEST(Future, Variant) {
    std::chrono::seconds t1, t2, t3, t4, t5;
    set_cpu_executor(1);
    auto* executor = folly::getCPUExecutor().get();
    ///EXPECT_NE(executor, nullptr);
    using variant = std::variant<
        folly::SemiFuture<int>,
        folly::Future<int>>;
    folly::Promise<int> p1;
    folly::Promise<int> p2;
    folly::Promise<int> p3;
    variant sf1 = p1.getSemiFuture();
    variant sf2 = p2.getSemiFuture();
    variant sf3 = p3.getSemiFuture();
}

TEST(Future, SharedPromise) {
    folly::SharedPromise<std::string> sp;
    auto sf1 = sp.getSemiFuture();
    auto sf2 = sp.getSemiFuture();
    EXPECT_THROW(sf1.value(), folly::FutureNotReady);
    sp.setValue("123"s);
    EXPECT_EQ(sf1.value(), "123"s);
    EXPECT_NE(&sf1.value(), &sf2.value());
    EXPECT_NE(sf1.value().c_str(), sf2.value().c_str());
}

TEST(Future, FutureSplitter) {
    folly::FutureSplitter<std::string> fs{ folly::makeFuture("123"s) };
    auto f1 = fs.getFuture();
    auto f2 = fs.getFuture();
    auto sf1 = fs.getSemiFuture();
    auto sf2 = fs.getSemiFuture();
    EXPECT_NE(&sf1.value(), &sf2.value());
    EXPECT_NE(&f1.value(), &f2.value());
    EXPECT_NE(f1.value().c_str(), f2.value().c_str());
    EXPECT_EQ(std::move(f1).get(), "123"s);
    EXPECT_EQ(std::move(f2).get(), "123"s);
    EXPECT_EQ(std::move(sf1).get(), "123"s);
    EXPECT_NE(fs.getFuture().get().c_str(), fs.getFuture().get().c_str());
    const char* d1 = nullptr;
    const char* d2 = nullptr;
    const char* d3 = nullptr;
    const char* d4 = nullptr;
    auto ff1 = fs.getFuture();
    auto ff2 = fs.getFuture();
    auto ff3 = fs.getFuture();
    auto ff4 = fs.getFuture();
    auto* p1 = std::move(ff1).then(
        [&](std::string&& str) {
            d1 = str.c_str();
            return std::addressof(str);
        }).get();
        auto* p2 = std::move(ff2).then(
            [&](std::string str) {
                d2 = str.c_str();
                return &str;
            }).get();
            auto* p3 = std::move(ff3).then(
                [&](std::string&& str) {
                    d3 = str.c_str();
                    return std::addressof(str);
                }).get();
                auto* p4 = std::move(ff4).then(
                    [&](std::string str) {
                        d4 = str.c_str();
                        return &str;
                    }).get();
                    EXPECT_NE(p1, p2);
                    EXPECT_NE(p1, p3);
                    EXPECT_EQ(p2, p4);      //?
                    EXPECT_NE(d1, d2);
                    EXPECT_NE(d1, d3);
                    EXPECT_EQ(d2, d4);
}

TEST(Future, Wait) {
    set_cpu_executor(1);
    folly::TimedDrivableExecutor executor;
    std::chrono::milliseconds t1, t2, t3, t4;
    folly::stop_watch<std::chrono::milliseconds> watch;
    auto f = folly::async(
        [&] {
            std::this_thread::sleep_for(100ms);
            t2 = watch.elapsed();
        }
    ).waitVia(&executor).then(
        folly::getCPUExecutor().get(),
        [&] {
            std::this_thread::sleep_for(100ms);
        });
        t1 = watch.elapsed();
        auto work = executor.drain();
        t3 = watch.elapsed();
        f.wait();
        t4 = watch.elapsed();
        EXPECT_EQ(work, 1);
}

TEST(Future, Filter) {
    auto f1 = folly::makeFuture(std::make_unique<std::string>("123"s));
    auto* p1 = &f1.value();
    auto* c1 = f1.value()->c_str();
    const char* c3 = nullptr;
    EXPECT_TRUE(f1.isReady());
    f1 = std::move(f1).filter(
        [&](auto& s) {
            c3 = std::data(*s);
            return true;
        });
    EXPECT_TRUE(f1.isReady());
    auto* p2 = &f1.value();
    auto* c2 = f1.value()->c_str();
    EXPECT_NE(p1, p2);
    EXPECT_EQ(c1, c2);
    EXPECT_EQ(c1, c3);
    f1 = std::move(f1).filter([&](auto&) { return false; });
    EXPECT_TRUE(f1.isReady());
    EXPECT_TRUE(f1.hasException());
    EXPECT_FALSE(f1.hasValue());
    EXPECT_THROW(f1.value(), folly::FuturePredicateDoesNotObtain);
    f1 = std::move(f1).onError(
        [](folly::FuturePredicateDoesNotObtain& exp) {
            return std::make_unique<std::string>("234");
        });
    EXPECT_STREQ(f1.value()->c_str(), "234");
    auto ff2 = folly::makeFuture("123"s);
    auto* pp2 = &ff2.value();
    const std::string* pp3 = nullptr;
    auto* cc2 = ff2.value().c_str();
    std::move(ff2).filter(
        [&](const std::string& s) {
            pp3 = &s;
            return true;
        });
    EXPECT_NE(pp2, pp3);
    EXPECT_EQ(pp2, &ff2.value());
    EXPECT_EQ(cc2, ff2.value().c_str());
    EXPECT_THROW(std::move(ff2).filter([&](const std::string& s) { return false; }), folly::FutureAlreadyContinued);
}

TEST(Future, Reduce) {
    std::vector<folly::Future<int>> fs;
    fs.push_back(folly::makeFuture(1));
    fs.push_back(folly::makeFuture(2));
    fs.push_back(folly::makeFuture(3));
    auto f = unorderedReduce(fs.begin(), fs.end(), 0.0,
                             [](double, int&& b) { return double(b); });
    EXPECT_TRUE(fs.front().isReady());
    EXPECT_TRUE(f.isReady());
    EXPECT_EQ(1, fs.front().value());
    EXPECT_EQ(3.0, std::move(f).get());
    EXPECT_THROW(unorderedReduce(fs.begin(), fs.end(), 0.0,
                                 [](double, int&& b) { return double(b); }),
                 folly::FutureAlreadyContinued);
}

TEST(Future, CollectN) {
    std::vector<folly::Future<std::string>> fs;
    fs.push_back(folly::makeFuture("1"));
    fs.push_back(folly::makeFuture("2"));
    fs.push_back(folly::makeFuture("3"));
    auto f = folly::collectN(fs, 2);
    auto v = std::move(f).get();
    EXPECT_STREQ(fs[0].value().c_str(), "");
    EXPECT_STREQ(fs[1].value().c_str(), "");
    EXPECT_STREQ(fs[2].value().c_str(), "3");
    EXPECT_FALSE(f.valid());
    EXPECT_THROW(folly::collectN(fs, 2), folly::FutureAlreadyContinued);
    EXPECT_THROW(folly::collectN(fs.begin() + 2, fs.end(), 1), folly::FutureAlreadyContinued);
}

TEST(Future, Map) {
    set_cpu_executor(1);
    auto executor = folly::getCPUExecutor();
    std::vector<folly::Future<std::string>> fs;
    folly::Promise<std::string> p1;
    folly::Promise<std::string> p2;
    fs.push_back(p1.getFuture());
    fs.push_back(p2.getFuture());
    const char *ptr1 = nullptr, *ptr2 = nullptr;
    EXPECT_TRUE(fs.front().valid());
    folly::InlineExecutor inline_executor;
    auto fs2 = folly::futures::map(inline_executor, fs,
                                   [&](std::string& s) {
                                       ptr2 = s.c_str();
                                   });
    EXPECT_FALSE(fs.front().valid());
    std::string str = "123";
    ptr1 = str.c_str();
    p1.setValue(std::move(str));
    EXPECT_EQ(ptr1, ptr2);
}

TEST(Beast, MultiBuffer) {
    std::array<char, 64> ar;
    std::fill_n(ar.begin(), ar.size(), 0xcc);
    boost::beast::multi_buffer b;
    EXPECT_EQ(b.size(), 0);
    b.commit(boost::asio::buffer_copy(
        b.prepare(64),
        boost::asio::buffer(ar)
    ));
    EXPECT_EQ(b.size(), 64);
    boost::beast::multi_buffer b2{ b };
    EXPECT_EQ(b2.size(), 64);
    auto cb1 = *b.data().begin();
    auto cb2 = *b2.data().begin();
    auto cb3 = cb1;
    EXPECT_EQ(cb1.data(), cb3.data());
    EXPECT_NE(cb1.data(), cb2.data());
    EXPECT_EQ(cb1.size(), cb2.size());
    boost::beast::multi_buffer b3{ std::move(b2) };
    EXPECT_EQ(b2.size(), 0);
    EXPECT_EQ(b3.size(), 64);
    auto cb4 = *b3.data().begin();
    EXPECT_EQ(cb4.data(), cb2.data());
}

TEST(DashManager, ParseMpdConfig) {
    set_cpu_executor(3);
    auto manager = net::component::dash_manager::async_create_parsed("http://localhost:8900/dash/tos_srd_4K.mpd").get();
    auto spatial_size = manager.scale_size();
    auto grid_size = manager.grid_size();
    EXPECT_EQ(grid_size, std::make_pair(3, 3));
    EXPECT_EQ(spatial_size, std::make_pair(3840, 1728));
}

TEST(DashManager, PathRegex) {
    auto path = "tile9-576p-1500kbps_dash$Number$.m4s"s;
    auto path_regex = [](std::string& path, auto index) {
        static const std::regex pattern{ "\\$Number\\$" };
        return std::regex_replace(path, pattern, fmt::to_string(index));
    };
    EXPECT_EQ(path_regex(path, 1), "tile9-576p-1500kbps_dash1.m4s");
    EXPECT_EQ(path_regex(path, 10), "tile9-576p-1500kbps_dash10.m4s");
}

TEST(Boost, CircularBuffer) {
    boost::circular_buffer<int> cb(2);
    EXPECT_EQ(cb.size(), 0);
    EXPECT_NE(cb.max_size(), 2);
    EXPECT_EQ(cb.capacity(), 2);
    cb.push_back(2);
    cb.push_front(1);
    EXPECT_EQ(cb.front(), 1);
    EXPECT_EQ(cb.at(0), 1);
    EXPECT_EQ(cb.at(1), 2);
    cb.push_front(3);
    EXPECT_EQ(cb.at(0), 3);
    EXPECT_EQ(cb.at(1), 1);
    cb.push_back(4);
    EXPECT_EQ(cb.at(0), 1);
    EXPECT_EQ(cb.at(1), 4);
    EXPECT_TRUE(cb.full());
}

TEST(Boost, Tribool) {
    using boost::logic::indeterminate;
    using boost::logic::tribool;
    tribool tb{ indeterminate };
    EXPECT_FALSE(bool{ tb });
    EXPECT_TRUE(indeterminate(tb == indeterminate));
    EXPECT_TRUE(indeterminate(tb));
    tribool tb2{ true };
    EXPECT_TRUE(bool{ tb2 });
    EXPECT_FALSE(indeterminate(tb2));
    tribool tb3{ false };
    EXPECT_FALSE(bool{ tb3 });
    EXPECT_FALSE(indeterminate(tb3));
}

TEST(Future, Poll) {
    auto[p, f] = folly::makePromiseContract<int>();
    auto poll = f.poll();
    EXPECT_FALSE(f.isReady());
    EXPECT_ANY_THROW(f.hasValue());
    EXPECT_THROW(f.hasException(), folly::FutureNotReady);
    EXPECT_FALSE(poll.has_value());
}

TEST(Std, ThrowNonException) {
    std::string* ptr = nullptr;
    try {
        auto sp = std::make_shared<std::string>("123");
        ptr = sp.get();
        throw std::move(sp);
    } catch (std::shared_ptr<std::string> sp) { // CopyConstructable & Destructible
        EXPECT_EQ(ptr, sp.get());
    }
}