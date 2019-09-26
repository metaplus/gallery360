#include "pch.h"
#include <concurrentqueue/blockingconcurrentqueue.h>
#include <concurrentqueue/concurrentqueue.h>
#include <readerwriterqueue/readerwriterqueue.h>

namespace moodycamel::test
{
    TEST(ConcurrentQueue, TryEnqueue) {
        ConcurrentQueue<int> q{ 20 };

        BlockingConcurrentQueue<int> q2;
        auto count = 0;
        ProducerToken t{ q };
        while (q.try_enqueue(t, 1)) {
            ++count;
        }
        EXPECT_EQ(count, 32);
        while (--count) {
            EXPECT_FALSE(q.try_enqueue(t, 2));
        }
        EXPECT_EQ(count, 0);
        while (++count <= 128) {
            EXPECT_TRUE(q.enqueue(3));
        }
        std::vector<int> v;
        EXPECT_EQ(q.try_dequeue_bulk_from_producer(t, std::back_inserter(v), 256), 32);
        EXPECT_EQ(v.size(), 32);
    }

    TEST(ReaderWriterQueue, Peek) {
        ReaderWriterQueue<int> q;
        q.enqueue(1);
        EXPECT_EQ(*q.peek(), 1);
        EXPECT_EQ(*q.peek(), 1);
        q.pop();
        EXPECT_TRUE(q.peek() == nullptr);
    }
}
