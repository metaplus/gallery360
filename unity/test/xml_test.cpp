#include "stdafx.h"
#include "CppUnitTest.h"
#include <tinyxml2.h>
#include <boost/multi_array.hpp>
#include <fstream>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace test
{
    TEST_CLASS(XmlTest) {

public:
    struct adaptation_set
    {
        int width = 0;
        int height = 0;
        int tile_row = 0;
        int tile_column = 0;
        inline static int frame_row = 0;
        inline static int frame_column = 0;
    };
    std::map<std::pair<int, int>, adaptation_set> info;

    TEST_METHOD(ParseAttribute) {
        std::ifstream file{ "D:/Media/dash/tos_srd_4K.mpd",std::ios::in | std::ios::binary };
        std::string text{ std::istreambuf_iterator<char>{file},std::istreambuf_iterator<char>{} };
        tinyxml2::XMLDocument document;
        auto errc = document.Parse(text.c_str());
        assert(errc == tinyxml2::XML_SUCCESS);
        auto mpd = document.FirstChildElement("MPD");
        assert(mpd != nullptr);
        auto period = mpd->FirstChildElement("Period");
        assert(period != nullptr);
        auto adaptation_set = period->FirstChildElement("AdaptationSet");
        assert(adaptation_set != nullptr);
        auto i = 0;
        do {
            auto supplemental_property = adaptation_set->FirstChildElement("SupplementalProperty");
            Logger::WriteMessage(std::to_string(++i).c_str());

        } while ((adaptation_set = adaptation_set->NextSiblingElement()) != nullptr);
    }
    };
}