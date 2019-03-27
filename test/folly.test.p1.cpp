#include "pch.h"
#include <folly/CancellationToken.h>

namespace folly::test
{
    TEST(CancellationTokenTest, DeregisteredCallbacksDontExecute) {
        CancellationSource src;
        bool executed1 = false;
        bool executed2 = false;
        CancellationCallback cb1{ src.getToken(), [&] { executed1 = true; } };
        {
            CancellationCallback cb2{ src.getToken(), [&] { executed2 = true; } };
        }
        src.requestCancellation();
        EXPECT_TRUE(executed1);
        EXPECT_FALSE(executed2);
    }

    TEST(CancellationTokenTest, CallbackThatDeregistersItself) {
        CancellationSource src;
        // Check that this doesn't deadlock when a callback tries to deregister
        // itself from within the callback.
        folly::Optional<CancellationCallback> callback;
        callback.emplace(src.getToken(), [&] { callback.clear(); });
        src.requestCancellation();
    }

    TEST(CancellationTokenTest, ManyCallbacks) {
        // This test checks that the CancellationSource internal state is able to
        // grow to accommodate a large number of callbacks and that there are no
        // memory leaks when it's all eventually destroyed.
        CancellationSource src;
        auto addLotsOfCallbacksAndWait = [t = src.getToken()] {
            int counter = 0;
            std::vector<std::unique_ptr<CancellationCallback>> callbacks;
            for (int i = 0; i < 100; ++i) {
                callbacks.push_back(
                    std::make_unique<CancellationCallback>(t, [&] { ++counter; }));
            }
            Baton<> baton;
            CancellationCallback cb{ t, [&] { baton.post(); } };
            baton.wait();
        };
        std::thread t1{ addLotsOfCallbacksAndWait };
        std::thread t2{ addLotsOfCallbacksAndWait };
        std::thread t3{ addLotsOfCallbacksAndWait };
        std::thread t4{ addLotsOfCallbacksAndWait };
        src.requestCancellation();
        t1.join();
        t2.join();
        t3.join();
        t4.join();
    }

    TEST(CancellationTokenTest, ManyConcurrentCallbackAddRemove) {
        auto runTest = [](CancellationToken ct) {
            auto cb = [] { std::this_thread::sleep_for(1ms); };
            while (!ct.isCancellationRequested()) {
                CancellationCallback cb1{ ct, cb };
                CancellationCallback cb2{ ct, cb };
                CancellationCallback cb3{ ct, cb };
                CancellationCallback cb5{ ct, cb };
                CancellationCallback cb6{ ct, cb };
                CancellationCallback cb7{ ct, cb };
                CancellationCallback cb8{ ct, cb };
            }
        };
        CancellationSource src;
        std::vector<std::thread> threads;
        for (int i = 0; i < 10; ++i) {
            threads.emplace_back([&, t = src.getToken()] { runTest(t); });
        }
        std::this_thread::sleep_for(1s);
        src.requestCancellation();
        for (auto& t : threads) {
            t.join();
        }
    }
}
