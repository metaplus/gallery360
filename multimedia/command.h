#pragma once
#include <filesystem>

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
            const int stride = 4;
            const int offset = 0;
        };

        static inline std::filesystem::path input_directory;
        static inline std::filesystem::path output_directory;
        static inline auto wcrop = 0;
        static inline auto hcrop = 0;
        static inline auto wscale = 1;
        static inline auto hscale = 1;

        static void resize(std::string_view input,
                           size_param size);

        static void crop_scale_transcode(std::filesystem::path input,
                                         rate_control rate = {},
                                         pace_control pace = {});

        static void crop_scale_package(std::filesystem::path input,
                                       int qp);

        static void package_container(rate_control rate);

        static void dash_segment(std::chrono::milliseconds duration);

        static void dash_segment(std::filesystem::path input,
                                 std::chrono::milliseconds duration);

        static std::filesystem::path merge_dash_mpd();

        static std::vector<std::filesystem::path> tile_path_list();
    };
}
