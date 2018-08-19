#include "stdafx.h"
#include "CppUnitTest.h"
#include <tinyxml2.h>
<<<<<<< HEAD
#include <boost/multi_array.hpp>
=======
>>>>>>> dev-module-plugin

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace test
{
<<<<<<< HEAD
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

    struct mpd
    {
        std::string title;
        std::chrono::duration<int64_t> duration;
    private:
        std::chrono::duration<int64_t> parse_total_duration();
        std::string parse_concrete_media();
    };

    TEST_METHOD(MultiArray) {

    }

    struct represent_base
    {
        int16_t id = 0;
        int bandwidth = 0;
        int time_scale = 0;
        std::string codecs;
        std::string mime_type;
        std::string initialization;
    };

    struct video_represent : represent_base
    {
        int8_t row = 0;
        int8_t column = 0;
        int16_t width = 0;
        int16_t height = 0;
    };

    struct audio_represent : represent_base
    {
        int sample_rate = 0;
    };

=======
    TEST_CLASS(XmlTest)
    {
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

        TEST_METHOD(ParseAttribute)
        {
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
            do
            {
                auto supplemental_property = adaptation_set->FirstChildElement("SupplementalProperty");
                Logger::WriteMessage(std::to_string(++i).c_str());

            } while ((adaptation_set = adaptation_set->NextSiblingElement()) != nullptr);
        }
>>>>>>> dev-module-plugin
    };
}