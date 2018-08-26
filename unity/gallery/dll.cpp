#include "stdafx.h"
#include "dll.h"

namespace dll
{
    std::unique_ptr<folly::NamedThreadFactory> create_thread_factory(std::string_view name_prefix) {
        return std::make_unique<folly::NamedThreadFactory>(name_prefix.data());
    }
}

namespace
{
    std::mutex resouce_mutex;
    std::map<int64_t, std::unique_ptr<dll::player_context>> player_contexts;
    std::atomic<int64_t> session_index = 0;
}

namespace unity
{
    void _nativeLibraryInitialize() {
        dll::net_module::initialize();
        std::atomic_store(&session_index, 0);
    }

    void _nativeLibraryRelease() {
        dll::net_module::release();
        player_contexts.clear();
    }

    INT64 _nativeMediaSessionCreateFileReader(LPCSTR url) {
        auto const ordinal = std::make_pair(0, 0);
        return -1;
    }

    INT64 _nativeMediaSessionCreateNetStream(LPCSTR url, INT row, INT column) {
        auto const ordinal = std::make_pair(row, column);
        auto index = std::atomic_fetch_add(&session_index, 1);
        folly::Uri uri{ url };
        auto player = std::make_unique<dll::player_context>(std::move(uri), ordinal);
        player_contexts.emplace(index, std::move(player));
        return index;
    }

    INT64 DLLAPI _nativeMediaSessionCreateDashStream(LPCSTR url, INT row, INT column, INT64 last_tile_index) {
        auto const ordinal = std::make_pair(row, column);
        auto index = std::atomic_fetch_add(&session_index, 1);
        folly::Uri uri{ url };
        net::protocal::dash protocal;
        protocal.last_tile_index = last_tile_index;
        auto player = std::make_unique<dll::player_context>(std::move(uri), ordinal, protocal);
        player_contexts.emplace(index, std::move(player));
        return index;
    }

    void _nativeMediaSessionRelease(INT64 id) {
        player_contexts.at(id)->deactive();
    }

    void _nativeMediaSessionGetResolution(INT64 id, INT& width, INT& height) {
        std::tie(width, height) = player_contexts.at(id)->resolution();
    }

    BOOL _nativeMediaSessionHasNextFrame(INT64 id) {
        return !player_contexts.at(id)->is_last_frame_taken();
    }
}

namespace dll::media_module
{
    media::frame try_take_decoded_frame(int64_t id) {
        return player_contexts.at(id)->take_decode_frame();
    }
}

namespace unity::debug
{
    BOOL _nativeMediaSessionDropFrame(INT64 id) {
        auto frame = dll::media_module::try_take_decoded_frame(id);
        return !frame.empty();
    }
}
