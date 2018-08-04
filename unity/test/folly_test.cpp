#include "stdafx.h"
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace test
{
    TEST_CLASS(FollyTest)
    {
    public:

        TEST_METHOD(Uri)
        {
            folly::Uri uri{ "http://www.facebook.com/foo/bar?key=foo#anchor" };
            Assert::AreEqual(uri.scheme().c_str(), "http");
            Assert::AreEqual(uri.host().c_str(), "www.facebook.com");
            Assert::AreEqual(uri.query().c_str(), "key=foo");
            Assert::AreEqual(uri.path().c_str(), "/foo/bar");
            Assert::AreEqual(uri.fragment().c_str(), "anchor");
            Assert::IsTrue(uri.getQueryParams().front() == std::make_pair("key"s, "foo"s));
            Assert::IsTrue(uri.getQueryParams().size() == 1);
        }
    };
}