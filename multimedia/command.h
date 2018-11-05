#pragma once

namespace media
{
    struct filter_param
    {
        int wcrop = 0;
        int hcrop = 0;
        int wscale = 1;
        int hscale = 1;
    };

    struct size_param
    {
        int width = 0;
        int height = 0;
    };

    struct rate_control
    {
        int bit_rate = 5000;
        int frame_rate = 30;
    };

    class command
    {
    public:
        struct pace_control
        {
            int stride = 16;
            int offset = 0;
        };

        static inline std::filesystem::path output_directory;

        static void resize(std::string_view input,
                           size_param size);

        static void crop_scale_transcode(std::string_view input,
                                         filter_param filter,
                                         rate_control rate = {},
                                         pace_control pace = {});

        static void package_container(rate_control rate);
    };
}