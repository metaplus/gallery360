#include "stdafx.h"
#include "CppUnitTest.h"
#pragma warning(disable:251)
#if 0
#include <boost/fiber/all.hpp>
#endif
using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace test
{
    TEST_CLASS(BoostTest) {
public:
#if 0
    TEST_METHOD(FiberChannel) {
        boost::fibers::use_scheduling_algorithm<boost::fibers::algo::round_robin>();
        boost::fibers::buffered_channel<int> x{ 20 };
    }
#endif
    };
}