#pragma once
#include "plugin.config.h"
#include "core/core.h"
#include <absl/strings/str_join.h>
#include <filesystem>
#include <OleAuto.h>

namespace util
{
    template <bool IsAnsiStandard = true>
    auto* unmanaged_string(std::string_view string) {
        if constexpr (IsAnsiStandard) {
            // allocate ansi string
            assert(string.size());
            auto* allocate = reinterpret_cast<LPSTR>(CoTaskMemAlloc(string.size() + 1));
            *std::copy(string.begin(), string.end(), allocate) = '\0';
            return allocate;
        } else {
            // allocate unicode string
            return SysAllocStringByteLen(string.data(),
                                         folly::to<UINT>(string.length()));
        }
    }

    inline auto make_log_directory(const std::filesystem::path& workset,
                                   const std::string& name_prefix) {
        const auto directory = workset / absl::StrJoin(
            { name_prefix, core::local_date_time("%Y%m%d.%H%M%S") }, ".");
        const auto result = create_directories(directory);
        assert(result && "trace create storage");
        return directory;
    }
}
