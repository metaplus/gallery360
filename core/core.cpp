#include "stdafx.h"
#include "core.h"

std::string core::time_string(std::string_view tformat, std::tm*(*tfunc)(const std::time_t*))
{
    std::ostringstream ostream;
    // const auto time_tmt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    const auto time_tmt = std::time(nullptr);
    const auto time_tm = *tfunc(&time_tmt);
    ostream << std::put_time(&time_tm, tformat.data());
    return ostream.str();
}

size_t core::count_entry(const std::filesystem::path& directory)
{
    //  non-recursive version, regardless of symbolic link
    const std::filesystem::directory_iterator iterator{ directory };
    return std::distance(begin(iterator), end(iterator));
}

size_t core::thread_hash_id(std::optional<std::thread::id> id)
{
    const std::hash<std::thread::id> hasher;
    return hasher(id.value_or(std::this_thread::get_id()));
}