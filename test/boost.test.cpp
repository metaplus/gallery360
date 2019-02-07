#include "pch.h"
#include <boost/asio.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/container/static_vector.hpp>
#include <boost/logic/tribool.hpp>
#include <boost/date_time/microsec_time_clock.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>
#include <boost/date_time/posix_time/time_parsers.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/process/environment.hpp>
#include <boost/process/system.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <folly/functional/Partial.h>

namespace boost::test
{
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

    TEST(CircularBuffer, Base) {
        boost::circular_buffer<int> cb(2);
        EXPECT_EQ(cb.size(), 0);
        EXPECT_NE(cb.max_size(), 2);
        EXPECT_EQ(cb.capacity(), 2);
        EXPECT_TRUE(cb.empty());
        cb.push_back(2);
        cb.push_front(1);
        EXPECT_TRUE(cb.full());
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

    TEST(CircularBuffer, Rotate) {
        boost::circular_buffer<int> cb(5);
        cb.push_back(3);
        cb.push_front(2);
        cb.push_back(4);
        EXPECT_EQ(cb.front(), 2);
        EXPECT_EQ(cb.back(), 4);
        cb.push_back(5);
        cb.push_front(1);
        EXPECT_EQ(cb.front(), 1);
        EXPECT_EQ(cb.back(), 5);
        EXPECT_EQ(cb.at(2), 3);
        EXPECT_TRUE(cb.full());
        cb.push_back(6);
        EXPECT_EQ(cb[0], 2);
        EXPECT_EQ(cb[1], 3);
        EXPECT_EQ(cb[2], 4);
        EXPECT_EQ(cb[3], 5);
        EXPECT_EQ(cb[4], 6);
        cb.push_front(0);
        EXPECT_EQ(cb[0], 0);
        EXPECT_EQ(cb[1], 2);
        EXPECT_EQ(cb[2], 3);
        EXPECT_EQ(cb[3], 4);
        EXPECT_EQ(cb[4], 5);
        const auto iter = cb.begin() + 3;
        EXPECT_EQ(4, *iter);
        cb.rotate(cb.begin() + 2);
        EXPECT_EQ(cb[0], 3);
        EXPECT_EQ(cb[1], 4);
        EXPECT_EQ(cb[2], 5);
        EXPECT_EQ(cb[3], 0);
        EXPECT_EQ(cb[4], 2);
        EXPECT_EQ(4, *iter);
        cb.rotate(iter);
        EXPECT_EQ(cb[0], 4);
        EXPECT_EQ(cb[1], 5);
        EXPECT_EQ(cb[2], 0);
        EXPECT_EQ(cb[3], 2);
        EXPECT_EQ(cb[4], 3);
        EXPECT_EQ(4, *iter);
    }

    TEST(CircularBuffer, Array) {
        boost::circular_buffer<int> cb{ 3 };
        cb.push_back(1);
        cb.push_back(2);
        cb.push_back(3);
        EXPECT_TRUE(cb.full());
        auto a1 = cb.array_one();
        auto a2 = cb.array_two();
        EXPECT_EQ(*a1.first, 1);
        EXPECT_EQ(a1.second, 3);
        EXPECT_EQ(a2.second, 0);
        EXPECT_TRUE(cb.is_linearized());
        cb.push_back(4);
        a1 = cb.array_one();
        a2 = cb.array_two();
        EXPECT_EQ(*a1.first, 2);
        EXPECT_EQ(a1.second, 2);
        EXPECT_EQ(a2.second, 1);
        cb.push_back(5);
        a1 = cb.array_one();
        a2 = cb.array_two();
        EXPECT_EQ(*a1.first, 3);
        EXPECT_EQ(a1.second, 1);
        EXPECT_EQ(a2.second, 2);
        cb.push_back(6);
        a1 = cb.array_one();
        a2 = cb.array_two();
        EXPECT_EQ(*a1.first, 4);
        EXPECT_EQ(a1.second, 3);
        EXPECT_EQ(a2.second, 0);
    }

    TEST(Tribool, Base) {
        using boost::logic::indeterminate;
        using boost::logic::tribool;
        tribool tb{ indeterminate };
        EXPECT_FALSE(bool{ tb });
        EXPECT_FALSE(bool{ tb == true });
        EXPECT_FALSE(bool{ tb == false });
        EXPECT_TRUE(indeterminate(tb == indeterminate));
        EXPECT_TRUE(indeterminate(tb));
        tribool tb2{ true };
        EXPECT_TRUE(bool{ tb2 });
        EXPECT_FALSE(indeterminate(tb2));
        tribool tb3{ false };
        EXPECT_FALSE(bool{ tb3 });
        EXPECT_FALSE(indeterminate(tb3));
    }

    TEST(DateTime, IO) {
        const auto time = boost::date_time::microsec_clock<boost::posix_time::ptime>::local_time();
        const auto time_str = fmt::format("{}", time);
        XLOG(INFO) << time;
        XLOG(INFO) << time_str;
        const auto time2 = boost::posix_time::time_from_string(time_str);
        EXPECT_EQ(time, time2);
    }

    TEST(InterProcess, FileLock) {
        std::filesystem::path lock_path = "F:/Debug/Interprocess/IPC.LOCK";
        if (create_directories(lock_path.parent_path()); is_regular_file(lock_path)) {
            ASSERT_TRUE(remove(lock_path));
        }
        EXPECT_NO_THROW(boost::interprocess::file_lock{});
        EXPECT_THROW(boost::interprocess::file_lock{ lock_path.string().c_str()},
                     boost::interprocess::interprocess_exception);
        {
            std::ofstream file{ lock_path, std::ios::trunc };
            EXPECT_TRUE(file.good());
        }
        EXPECT_EQ(file_size(lock_path), 0);
        boost::interprocess::file_lock file_lock{ lock_path.string().c_str() };
        auto worker = std::thread{
            [&file_lock] {
                file_lock.lock_sharable();
                file_lock.unlock();
                file_lock.lock();
                file_lock.unlock();
            }
        };
        file_lock.lock();
        file_lock.unlock();
        file_lock.lock_sharable();
        file_lock.unlock();
        worker.join();
    }

    TEST(Process, Environment) {
        auto t = boost::this_process::environment()["TraceDb"];
        EXPECT_FALSE(t.empty());
        EXPECT_EQ(t.get_name(), "TraceDb");
        EXPECT_EQ(t.to_string(), "F:\\TraceDb");
        EXPECT_EQ(t.to_vector().size(), 1);
        EXPECT_EQ(t.to_vector().front(), "F:\\TraceDb");
    }

    TEST(Asio, DeferContext) {
        boost::asio::io_context ctx;
        std::thread::id id0, id1, id2, id3;
        EXPECT_EQ(id0, id1);
        EXPECT_EQ(id0, id2);
        auto work = [&ctx] {
            ctx.run();
        };
        auto work_guard = boost::asio::make_work_guard(ctx);
        std::thread th1{ work };
        std::thread th2{ work };
        folly::stop_watch<milliseconds> w;
        boost::asio::post(ctx, [&] {
            id1 = std::this_thread::get_id();
            EXPECT_TRUE(ctx.get_executor().running_in_this_thread());
            boost::asio::defer(ctx, [&] {
                id3 = std::this_thread::get_id();
                EXPECT_GE(w.elapsed(), 50ms);
                EXPECT_LT(w.elapsed(), 60ms);
                std::this_thread::sleep_for(50ms);
            });
            EXPECT_GE(w.elapsed(), 0ms);
            EXPECT_LT(w.elapsed(), 10ms);
            std::this_thread::sleep_for(100ms);
        });
        boost::asio::post(ctx, [&] {
            id2 = std::this_thread::get_id();
            std::this_thread::sleep_for(50ms);
        });
        std::this_thread::sleep_for(200ms);
        EXPECT_NE(id0, id1);
        EXPECT_NE(id0, id2);
        EXPECT_NE(id1, id2);
        EXPECT_EQ(id2, id3);
        work_guard.reset();
        th1.join();
        th2.join();
    }

    TEST(Asio, DeferStrand) {
        boost::asio::io_context ctx;
        boost::asio::strand<decltype(ctx)::executor_type> st{ ctx.get_executor() };
        std::thread::id id0, id1, id2;
        milliseconds t1, t2;
        EXPECT_EQ(id0, id1);
        EXPECT_EQ(id0, id2);
        auto work = [&ctx] {
            ctx.run();
        };
        auto work_guard = boost::asio::make_work_guard(ctx);
        std::thread th1{ work };
        std::thread th2{ work };
        std::thread th3{ work };
        folly::stop_watch<milliseconds> w;
        boost::asio::post(ctx, [&] {
            id1 = std::this_thread::get_id();
            EXPECT_TRUE(ctx.get_executor().running_in_this_thread());
            boost::asio::dispatch(ctx, [&] {
                EXPECT_EQ(id1, std::this_thread::get_id());
                boost::asio::defer(st, [&] {
                    t1 = w.elapsed();
                    std::this_thread::sleep_for(50ms);
                });
                std::this_thread::sleep_for(50ms);
            });
            EXPECT_GE(w.elapsed(), 50ms);
            EXPECT_LT(w.elapsed(), 60ms);
            std::this_thread::sleep_for(200ms);
        });
        boost::asio::post(ctx, [&] {
            id2 = std::this_thread::get_id();
            boost::asio::defer(st, [&] {
                t2 = w.elapsed();
                std::this_thread::sleep_for(50ms);
            });
            std::this_thread::sleep_for(200ms);
        });
        std::this_thread::sleep_for(300ms);
        EXPECT_NE(id0, id1);
        EXPECT_NE(id0, id2);
        EXPECT_NE(id1, id2);
        EXPECT_GE(std::min(t1, t2), 0ms);
        EXPECT_LT(std::min(t1, t2), 10ms);
        EXPECT_GE(std::max(t1, t2), 50ms);
        EXPECT_LT(std::max(t1, t2), 60ms);
        work_guard.reset();
        th1.join();
        th2.join();
        th3.join();
    }

    TEST(Asio, DispatchStrand) {
        boost::asio::io_context ctx;
        boost::asio::strand<decltype(ctx)::executor_type> st{ ctx.get_executor() };
        std::thread::id id0, id1, id2;
        milliseconds t1, t2, t3, t4;
        EXPECT_EQ(id0, id1);
        EXPECT_EQ(id0, id2);
        auto work = [&ctx] {
            ctx.run();
        };
        auto work_guard = boost::asio::make_work_guard(ctx);
        std::thread th1{ work };
        std::thread th2{ work };
        folly::stop_watch<milliseconds> w;
        boost::asio::post(ctx, [&] {
            id1 = std::this_thread::get_id();
            EXPECT_TRUE(ctx.get_executor().running_in_this_thread());
            boost::asio::dispatch(ctx, [&] {
                // as-if boost::asio::defer, no blocking triggered in contest
                EXPECT_EQ(id1, std::this_thread::get_id());
                std::this_thread::sleep_for(1ms);
                boost::asio::dispatch(st, [&] {
                    EXPECT_EQ(id2, std::this_thread::get_id());
                    t1 = w.elapsed();
                    std::this_thread::sleep_for(50ms);
                });
                t3 = w.elapsed();
                std::this_thread::sleep_for(50ms);
            });
            EXPECT_GE(w.elapsed(), 50ms);
            EXPECT_LT(w.elapsed(), 60ms);
            std::this_thread::sleep_for(200ms);
        });
        boost::asio::post(ctx, [&] {
            id2 = std::this_thread::get_id();
            boost::asio::dispatch(st, [&] {
                t2 = w.elapsed();
                std::this_thread::sleep_for(50ms);
            });
            t4 = w.elapsed();
            std::this_thread::sleep_for(200ms);
        });
        std::this_thread::sleep_for(500ms);
        EXPECT_NE(id0, id1);
        EXPECT_NE(id0, id2);
        EXPECT_NE(id1, id2);
        EXPECT_GE(t2, 0ms);
        EXPECT_LT(t2, 10ms);
        EXPECT_GE(t4, 50ms);
        EXPECT_LT(t4, 60ms);
        EXPECT_GE(t3, 1ms);
        EXPECT_LT(t3, 11ms);
        EXPECT_GE(t1, 250ms);
        EXPECT_LT(t1, 260ms);
        work_guard.reset();
        th1.join();
        th2.join();
    }

    TEST(Container, StaticVector) {
        struct bulk
        {
            char c[512];
        };
        EXPECT_NO_THROW((container::static_vector<bulk, 1024>{}));
    }
}
