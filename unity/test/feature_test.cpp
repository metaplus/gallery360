#include "stdafx.h"
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace test
{
    TEST_CLASS(FeatureTest) {
public:

    TEST_METHOD(StructuredBinding) {
        auto[x, y, z] = std::make_tuple(1, 2, 3);
        Assert::AreEqual(x, 1);
    }

    TEST_METHOD(RegexReplace) {
        std::string sample = "tile1-576p-1500kbps_dash$Number$.m4s";
        std::string replace = std::regex_replace(sample,std::regex{"\\$Number\\$"},"{}");
    }
    };
}