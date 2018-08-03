#include "stdafx.h"
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace test
{		
	TEST_CLASS(FeatureTest)
	{
	public:
		
		TEST_METHOD(StructuredBinding)
		{
            auto[x, y, z] = std::make_tuple(1, 2, 3);
            Assert::AreEqual(x, 1);
		}
	};
}