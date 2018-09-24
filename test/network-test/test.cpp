#include "pch.h"
#include <boost/beast/core/multi_buffer.hpp>
#include <folly/executors/Async.h>
#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/executors/GlobalExecutor.h>
#include <folly/executors/task_queue/LifoSemMPMCQueue.h>
#include <folly/executors/task_queue/UnboundedBlockingQueue.h>
#include <folly/Function.h>
#include <folly/futures/Future.h>
#include <folly/futures/SharedPromise.h>
#include <folly/MoveWrapper.h>
#include <folly/stop_watch.h>
#include <folly/system/ThreadName.h>
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

TEST(LifoSemMPMCQueue, AddCapacity) {
    folly::LifoSemMPMCQueue<int, folly::QueueBehaviorIfFull::THROW> queue{ 1 };
    queue.add(1); folly::Promise<int> p;
    EXPECT_THROW(queue.add(1), std::runtime_error);
}

TEST(Promise, GetSemiFuture) {
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

TEST(Future, SemiFutureViaExecutor) {
    std::chrono::seconds t1, t2, t3, t4, t5;
    int y = 0;
    auto executor = std::make_shared<folly::CPUThreadPoolExecutor>(3);
    folly::Promise<folly::Unit> p;
    folly::setCPUExecutor(executor);
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

auto config_executor = [](auto concurrency) {
    set_cpu_executor(concurrency);
    auto* executor = folly::getCPUExecutor().get();
    EXPECT_NO_THROW(dynamic_cast<folly::CPUThreadPoolExecutor&>(*executor));
    return executor;
};

TEST(Future, SemiFutureDefer) {
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

TEST(Future, SemiFutureVariant) {
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
    folly::stop_watch<std::chrono::seconds> watch;
}

TEST(Future, SharedPromise) {
    folly::SharedPromise<int> sp;
    auto sf1 = sp.getSemiFuture();
    auto sf2 = sp.getSemiFuture();
    EXPECT_THROW(sf1.value(), folly::FutureNotReady);
    sp.setValue(1);
    EXPECT_EQ(sf2.value(), 1);
}

TEST(DashManager, ParseMpdConfig) {
    set_cpu_executor(3);
    auto manager = net::component::dash_manager::async_create_parsed("http://localhost:8900/dash/tos_srd_4K.mpd").get();
    auto spatial_size = manager.scale_size();
    auto grid_size = manager.grid_size();
    EXPECT_EQ(grid_size, std::make_pair(3, 3));
    EXPECT_EQ(spatial_size, std::make_pair(3840, 1728));
}

TEST(Beast, MultiBuffer) {
    boost::beast::multi_buffer b;
    EXPECT_EQ(b.size(), 0);
}