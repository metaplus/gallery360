#include "stdafx.h"
#include "CppUnitTest.h"
#include <boost/thread/future.hpp>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace test
{
    TEST_CLASS(BoostTest) {
public:

    TEST_METHOD(AsyncConcurrencyAheadBlock) {
        std::atomic<int> count = 0;
        auto loop_count = 0;
        std::vector<boost::future<void>> container;
        folly::Synchronized<int> sync{ 1 };
        std::vector<decltype(sync)::LockedPtr> locks;
        std::set<boost::thread::id> id_set;
        container.reserve(1024);
        while (++loop_count != 50) {
            container.push_back(boost::async(
                //boost::launch::async,
                [&] {
                    //locks.push_back(sync.wlock());
                    id_set.emplace(boost::this_thread::get_id());
                    auto message = fmt::format("thread {} count {}", boost::this_thread::get_id(), ++count);
                    Logger::WriteMessage(message.c_str());
                    boost::this_thread::sleep_for(boost::chrono::hours{ 1 });
                }));
        }
        boost::this_thread::sleep_for(boost::chrono::seconds{ 1 });
        boost::wait_for_all(container.begin(), container.end());
        boost::this_thread::sleep_for(boost::chrono::hours{ 1 });
    }
    TEST_METHOD(AsyncLaunchPolicy) {
        auto get_id = [] { return boost::this_thread::get_id(); };
        boost::thread::id id1, id2, id3, id4, id0 = boost::this_thread::get_id();
        auto future = boost::async(boost::launch::async, [&] { id1 = get_id(); })
            .then(boost::launch::async, [&](auto f) { id2 = get_id(); })
            .then(boost::launch::sync, [&](auto f) { id3 = get_id(); })
            .then(boost::launch::deferred, [&](auto f) { id4 = get_id(); });
        future.get();
    }
    };
}