#pragma once

namespace media
{
    struct filter_param final
    {
        const int wcrop = 0;
        const int hcrop = 0;
        const int wscale = 1;
        const int hscale = 1;
    };

    struct size_param final
    {
        const int width = 0;
        const int height = 0;
    };

    struct rate_control final
    {
        const int bit_rate = 5000;
        const int frame_rate = 30;
    };

    class command final
    {
    public:
        struct pace_control final
        {
            const int stride = 16;
            const int offset = 0;
        };

        static inline std::filesystem::path output_directory;

        static void resize(std::string_view input,
                           size_param size);

        static void crop_scale_transcode(std::string_view input,
                                         filter_param filter,
                                         rate_control rate = {},
                                         pace_control pace = {});

        static void package_container(rate_control rate);

        static void dash_segment(std::chrono::milliseconds duration);
    };
}