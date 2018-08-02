#include "stdafx.h"
#include "dll.h"

namespace dll
{
    std::unique_ptr<folly::NamedThreadFactory> create_thread_factory(std::string_view name_prefix)
    {
        return std::make_unique<folly::NamedThreadFactory>(name_prefix.data());
    }

}

namespace unity::internal
{
    std::mutex resouce_mutex;
    std::map<int64_t, std::unique_ptr<dll::player_context>> player_contexts;
    std::atomic<int64_t> session_index = 0;
}

namespace unity
{
    void _nativeLibraryInitialize()
    {
        dll::net_module::initialize();
    }

    void _nativeLibraryRelease()
    {
        dll::net_module::release();
    }

    INT64 _nativeMediaSessionCreateFileReader(LPCSTR url)
    {
        auto const ordinal = std::make_pair(0, 0);
        return -1;
    }

    INT64 _nativeMediaSessionCreateNetStream(LPCSTR url, INT row, INT column)
    {
        auto const ordinal = std::make_pair(row, column);
        auto const[host, target] = dll::net_module::split_url_components(url);
        auto session_index = std::atomic_fetch_add(&internal::session_index, 1);
        internal::player_contexts.emplace(session_index, std::make_unique<dll::player_context>(host, target, ordinal));
        return session_index;
    }

    void _nativeMediaSessionRelease(INT64 id)
    {
        internal::player_contexts.at(id)->deactive();
    }

    void _nativeMediaSessionGetResolution(INT64 id, INT& width, INT& height)
    {
        std::tie(width, height) = internal::player_contexts.at(id)->resolution();
    }

    BOOL _nativeMediaSessionTryUpdateFrame(INT64 id)
    {
        auto& player = *internal::player_contexts.begin();
        return false;
        //return !player.is_codec_complete() || player.available_size() > 0;
    }

    BOOL unity::_nativeMediaSessionHasNextFrame(INT64 id)
    {
        return !internal::player_contexts.begin()->second->is_last_frame_taken();
    }

}

namespace dll::media_module
{
    media::frame try_take_decoded_frame(int64_t id)
    {
        return  unity::internal::player_contexts.begin()->second->take_decode_frame();
    }
}

namespace unity::debug
{
    BOOL _nativeMediaSessionDropFrame(INT64 id)
    {
        auto frame = dll::media_module::try_take_decoded_frame(id);
        return !frame.empty();
    }
}
