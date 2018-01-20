#pragma once


namespace tag
{
    struct url
    {   
        static constexpr std::string_view name="media url"sv;
        template<typename Url>
        static std::string message(Url&& url)
        {
            using namespace std::experimental::filesystem;
            if constexpr (std::is_convertible_v<Url,std::string>)
                return absolute(path{url}).generic_string(); 
            else if constexpr (std::is_convertible_v<Url,path>)
                return url.generic_string();
            else
                static_assert(false,"taste undesired type");
        }
    };
    struct fps
    {
        static constexpr std::string_view name="rendering fps {}"sv;
        static std::string message(std::string_view str)
        {
            return fmt::format(name.data(),str);
        }
    };
}