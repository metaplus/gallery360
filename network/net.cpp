#include "stdafx.h"
#include "net.hpp"

std::string net::config_path(core::as_view_t) noexcept
{
    return config_path().generic_string();
}

std::filesystem::path net::config_path() noexcept
{
    auto const config_dir_cstr = _NET_DIR;
    auto const config_path = std::filesystem::path{ config_dir_cstr } / "config.xml";
    core::verify(is_directory(std::filesystem::path{ config_dir_cstr }));
    core::verify(is_regular_file(config_path));
    return config_path;
}

boost::property_tree::ptree const& net::config()
{
    static std::once_flag flag;
    static boost::property_tree::ptree config_ptree;
    std::call_once(flag, []
                   {
                       auto const config_path = net::config_path(core::as_view);
                       boost::property_tree::read_xml(config_path, config_ptree);
                   });
    return config_ptree;
}
