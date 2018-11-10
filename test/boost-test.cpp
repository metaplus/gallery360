#include "pch.h"
#include <boost/circular_buffer.hpp>
#include <boost/logic/tribool.hpp>

namespace boost_test
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

    TEST(Container, CircularBuffer) {
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

    TEST(State, Tribool) {
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
}
