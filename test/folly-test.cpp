#include "pch.h"
#include <folly/dynamic.h>
#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/executors/task_queue/UnboundedBlockingQueue.h>
#include <folly/json.h>
#include <folly/stop_watch.h>
#include <boost/beast/core/ostream.hpp>

using std::chrono::milliseconds;
using std::chrono::steady_clock;
using boost::beast::multi_buffer;

namespace folly_test
{
    TEST(Executor, ThreadedExecutor) {
        uint64_t i1 = 0, i2 = 0, i3 = 0;
        auto i0 = folly::getCurrentThreadID();
        folly::stop_watch<milliseconds> watch;
        {
            auto executor = core::make_threaded_executor("Test");
            watch.reset();
            executor->add([&] {
                std::this_thread::sleep_for(200ms);
                i1 = folly::getCurrentThreadID();
            });
            executor->add([&] {
                std::this_thread::sleep_for(150ms);
                i2 = folly::getCurrentThreadID();
            });
            executor->add([&] {
                std::this_thread::sleep_for(100ms);
                i3 = folly::getCurrentThreadID();
            });
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

    TEST(Executor, MakePool) {
        auto executor = core::make_pool_executor(10);
        EXPECT_EQ(executor->numActiveThreads(), 1);
        EXPECT_EQ(executor->numThreads(), 10);
        EXPECT_EQ(executor->getPendingTaskCount(), 0);
        EXPECT_STREQ(executor->getName().data(), "CorePool");
        EXPECT_EQ(executor->getPoolStats().idleThreadCount, 10);
        EXPECT_EQ(executor->getPoolStats().activeThreadCount, 0);
    }

    TEST(Future, Exchange) {
        auto f = folly::makeFuture(1);
        auto i = std::exchange(f, folly::makeFuture(2)).value();
        EXPECT_EQ(i, 1);
        auto i2 = std::exchange(f, folly::makeFuture(3)).get();
        EXPECT_EQ(i2, 2);
        EXPECT_EQ(f.value(), 3);
    }

    TEST(Function, Base) {
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

    TEST(MoveWrapper, Base) {
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

    TEST(Queue, LifoSemMPMCQueue) {
        folly::LifoSemMPMCQueue<int, folly::QueueBehaviorIfFull::THROW> queue{ 1 };
        queue.add(1);
        folly::Promise<int> p;
        EXPECT_THROW(queue.add(1), std::runtime_error);
    }

    TEST(Queue, UnboundedQueue) {
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
            std::vector<int> v{ 1, 2, 3, 4, 5 };
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

    TEST(Queue, MPMCQueue) {
        folly::MPMCQueue<int> q{ 2 };
        EXPECT_EQ(q.capacity(), 2);
        EXPECT_TRUE(q.isEmpty());
        folly::Baton<> b;
        decltype(steady_clock::now()) t1, t2, t3, t4;
        std::thread{
            [&] {
                q.blockingWrite(1);
                EXPECT_TRUE(q.write(2));
                EXPECT_FALSE(q.writeIfNotFull(3));
                EXPECT_TRUE(q.isFull());
                t1 = steady_clock::now();
                b.post();
                std::this_thread::sleep_for(100ms);
                q.blockingWrite(3);
            }
        }.detach();
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

    TEST(Future, Promise) {
        folly::Promise<int> p;
        auto sf = p.getSemiFuture()
                   .deferValue([](int x) {
                       return std::to_string(x) + "123";
                   });
        p.setWith([] {
            return 456;
        });
        EXPECT_STREQ(std::move(sf).get().c_str(), "456123");
    }

    TEST(Future, CollectAllSemiFuture) {
        auto c1 = folly::collectAllSemiFuture(
            folly::makeSemiFuture(1),
            folly::makeSemiFutureWith([] {
                return 2;
            })
        );
        auto [r10, r11] = std::move(c1).get();
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
                  .deferValue([](auto) {
                      return 2;
                  })
                  .via(executor.get())
                  .thenValue(
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
        auto [p, sf] = folly::makePromiseContract<int>();
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
        auto* p1 = std::move(ff1).thenValue(
            [&](std::string&& str) {
                d1 = str.c_str();
                return std::addressof(str);
            }).get();
        auto* p2 = std::move(ff2).thenValue(
            [&](std::string str) {
                d2 = str.c_str();
                return &str;
            }).get();
        auto* p3 = std::move(ff3).thenValue(
            [&](std::string&& str) {
                d3 = str.c_str();
                return std::addressof(str);
            }).get();
        auto* p4 = std::move(ff4).thenValue(
            [&](std::string str) {
                d4 = str.c_str();
                return &str;
            }).get();
        EXPECT_NE(p1, p2);
        EXPECT_NE(p1, p3);
        EXPECT_EQ(p2, p4); //?
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
        f1 = std::move(f1).filter([&](auto&) {
            return false;
        });
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
                                 [](double, int&& b) {
                                     return double(b);
                                 });
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

    TEST(Future, Poll) {
        auto [p, f] = folly::makePromiseContract<int>();
        auto poll = f.poll();
        EXPECT_FALSE(f.isReady());
        EXPECT_ANY_THROW(f.hasValue());
        EXPECT_THROW(f.hasException(), folly::FutureNotReady);
        EXPECT_FALSE(poll.has_value());
    }

    TEST(Future, DeferError) {
        static_assert(std::is_base_of<std::exception, core::bad_request_error>::value);
        static_assert(std::is_base_of<std::exception, core::not_implemented_error>::value);
        auto sf = folly::makeSemiFutureWith([] {
            throw core::bad_request_error{ "123" };
            return 1;
        });
        EXPECT_TRUE(sf.hasException());
        EXPECT_FALSE(sf.hasValue());
        EXPECT_THROW(sf.value(), core::bad_request_error);
        sf = std::move(sf).deferError([](auto&&) {
            throw core::not_implemented_error{ "123" };
            return 2;
        });
        EXPECT_THROW(sf.value(), folly::FutureNotReady);
        EXPECT_THROW(std::move(sf).get(), core::not_implemented_error);
        auto [p1, sf1] = folly::makePromiseContract<int>();
        p1.setException(core::bad_request_error{ "123" });
        EXPECT_TRUE(sf1.hasException());
        sf1 = std::move(sf1).deferError([](auto&&) {
            return 2;
        });
        EXPECT_THROW(sf1.value(), folly::FutureNotReady);
        EXPECT_EQ(std::move(sf1).get(), 2);
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
            boost::beast::ostream(buffer) << std::ifstream{ "F:/Gpac/debug/NewYork/5k/segment_2_2_5k_init.mp4", std::ios::binary }.rdbuf();
            EXPECT_EQ(buffer.size(), 925); // ~ 1kB
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
                        t = watch.elapsed(); // ~ 10us
                        auto* data = (*fss.value().data().begin()).data();
                        auto fss2 = fs.getSemiFuture();
                        auto* data2 = (*fss2.value().data().begin()).data();
                        EXPECT_EQ(fss.value().size(), 925);
                        EXPECT_NE(data, data2);
                        if (std::exchange(last, data)) {
                            EXPECT_EQ(data, last);
                            EXPECT_NE(data, (*buffer.data().begin()).data());
                        }
                        //std::cout << t;
                        fmt::print("elapsed: {}\n", t.count());
                    }
                    {
                        watch.reset();
                        auto fss = fs2.getSemiFuture();
                        auto t2 = watch.elapsed(); // ~ 2us
                        EXPECT_GT(t, t2);
                        fmt::print("elapsed-ptr: {}\n", t2.count());
                    }
                }
            }
        }
    }

    TEST(ForEach, Base) {
        auto one = std::make_tuple(1, 2, 3);
        auto two = std::vector<int>{ 1, 2, 3 };
        const auto func = [](auto element, auto index) {
            std::cout << index << " : " << element << std::endl;
        };
        folly::for_each(one, func);
        folly::for_each(two, func);
    }

    TEST(ForEach, Control) {
        auto range_one = std::vector<int>{ 1, 2, 3 };
        auto range_two = std::make_tuple(1, 2, 3);
        const auto func = [](auto ele, auto index) {
            std::cout << "Element at index " << index << " : " << ele << std::endl;
            if (index == 1) {
                return folly::loop_break;
            }
            return folly::loop_continue;
        };
        folly::for_each(range_one, func);
        folly::for_each(range_two, func);

    }

    TEST(ForEach, Fetch) {
        auto range_one = std::make_tuple(1, 2, 3);
        auto range_two = std::make_tuple(4, 5, 6);
        folly::for_each(range_one, [&range_two](auto ele, auto index) {
            folly::fetch(range_two, index) = ele;
        });
    }

    TEST(Try, Base) {
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
            EXPECT_TRUE(t.hasValue()); //! default constructed state after moved out, not empty
            EXPECT_FALSE(t.value() != nullptr);
            EXPECT_TRUE(t2.hasValue());
            EXPECT_EQ(*t2.value(), 2);
        }
    }

    TEST(Dynamic,Json) {
        auto document = R"({"key":12,"key2":[false,null,true,"yay"]})"s;
        auto parsed = folly::parseJson(document);
        EXPECT_EQ(parsed["key"], 12);
        EXPECT_EQ(parsed["key2"][0], false);
        EXPECT_EQ(parsed["key2"][1], nullptr);
        folly::dynamic dyn = folly::dynamic::object
            ("key2", folly::dynamic::array(false, nullptr, true, "yay"))
            ("key", 12);
        EXPECT_EQ(folly::toJson(dyn), document);
    }
}
