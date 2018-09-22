#include "pch.h"
#include <folly/MoveWrapper.h>
#include <folly/executors/task_queue/LifoSemMPMCQueue.h>
#include <folly/futures/Future.h>
#include <folly/system/ThreadName.h>
#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/executors/GlobalExecutor.h>
#include <variant>

namespace folly
{
    TEST(MoveWrapper, Copy) {
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
}

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...)->overloaded<Ts...>;

#pragma warning(disable:244 267)
namespace core
{
    TEST(Overload, Variant) {
        using variant = std::variant<int, long, double, std::string>;
        variant v{ 1 };
        auto r1 = std::visit(overloaded{
            [](auto arg) -> int { return arg * 2; },
            [](double arg) -> int { return arg + 0.1; },
            [](const std::string& arg) -> int { return arg.size(); } },
            variant{ 1 });
        EXPECT_EQ(r1, 2);
        auto r2 = std::visit(overloaded{
            [](auto arg) -> int { return arg * 2; },
            [](double arg) -> int { return arg + 0.1; },
            [](const std::string& arg) -> int { return arg.size(); } },
            variant{ 1.2 });
        EXPECT_EQ(r2, 1);
        auto r3 = std::visit(overloaded{
            [](auto arg) -> int { return arg * 2; },
            [](double arg) -> int { return arg + 0.1; },
            [](const std::string& arg) -> int { return arg.size(); } },
            variant{ std::string{"123"} });
        EXPECT_EQ(r3, std::string{ "123" }.size());
    }

    void set_cpu_executor(int concurrency, int queue_size = 4096, std::string_view pool_name = "CorePool") {
        static auto executor = std::make_shared<folly::CPUThreadPoolExecutor>(
            std::make_pair(concurrency, 1),
            std::make_unique<folly::LifoSemMPMCQueue<folly::CPUThreadPoolExecutor::CPUTask, folly::QueueBehaviorIfFull::BLOCK>>(queue_size),
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
}