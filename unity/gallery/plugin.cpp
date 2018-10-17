#include "stdafx.h"
#include "export.h"
#include "network/component.h"
#include "multimedia/component.h"
#include "multimedia/pch.h"
#include "graphic.h"

using net::component::dash_manager;
using net::component::frame_consumer;
using net::component::frame_builder;
using net::component::frame_indexed_builder;
using net::component::ordinal;
using media::component::frame_segmentor;
using media::component::pixel_array;
using media::component::pixel_consume;
using boost::beast::multi_buffer;

struct texture_context;

std::shared_ptr<folly::ThreadPoolExecutor> event_executor;
std::shared_ptr<folly::ThreadPoolExecutor> session_executor;
folly::Future<dash_manager> manager = folly::Future<dash_manager>::makeEmpty();

pixel_consume pixel_update;
pixel_array pixels;
media::frame tile_frame{ nullptr };

const auto asio_concurrency = std::thread::hardware_concurrency() / 2;
const auto executor_concurrency = std::thread::hardware_concurrency();
const auto decoder_concurrency = 4u;

auto unity_time = 0.f;
std::shared_ptr<dll::graphic> dll_graphic;
UnityGfxRenderer unity_device = kUnityGfxRendererNull;
IUnityInterfaces* unity_interface = nullptr;
IUnityGraphics* unity_graphics = nullptr;
std::map<std::pair<int, int>, texture_context> tile_textures;

namespace state
{
    auto tile_col = 0;
    auto tile_row = 0;
    decltype(tile_textures)::iterator texture_iterator;
}

struct texture_context
{
    std::array<ID3D11Texture2D*, 3> texture{};
    folly::USPSCQueue<media::frame, true> frame_queue{};
    int x = 0;
    int y = 0;
    bool stop = false;
};

struct tile_media_context
{
    frame_segmentor segmentor;
    multi_buffer tile_buffer;
};

auto texture_at = [](int col, int row, bool construct = false) -> decltype(auto) {
    if (construct) {
        return tile_textures[std::make_pair(col, row)];
    }
    state::texture_iterator = tile_textures.find(std::make_pair(col, row));
    assert(state::texture_iterator != tile_textures.end());
    return (state::texture_iterator->second);
};

std::pair<int, int> frame_grid;
std::pair<int, int> frame_scale;
auto tile_width = 0;
auto tile_height = 0;

auto tile_index = [](int x, int y) constexpr {
    return x + y * frame_grid.first + 1;
};

auto tile_ordinal = [](int index) constexpr {
    const auto x = (index - 1) % frame_grid.first;
    const auto y = (index - 1) / frame_grid.first;
    return std::make_pair(x, y);
};

namespace debug
{
    constexpr auto enable_null_texture = true;
    constexpr auto enable_dump = false;
    constexpr auto enable_legacy = false;
    constexpr auto pixel_dump_col = 3;
    constexpr auto pixel_dump_width = 1280;
    constexpr auto pixel_dump_height = 640;
}

namespace unity
{
    void DLLAPI unity::_nativeConfigExecutor() {
        if (!event_executor) {
            event_executor = core::set_cpu_executor(executor_concurrency, "PluginPool");
        }
        assert(!session_executor);
    }

    frame_builder gradual_frame_builder(unsigned concurrency) {
        return [concurrency](multi_buffer& head_buffer,
                             multi_buffer&& tail_buffer)
            -> frame_consumer {
            frame_segmentor segmentor{ core::split_buffer_sequence(head_buffer,tail_buffer),concurrency };
            return[segmentor = std::move(segmentor), tail_buffer = std::move(tail_buffer)]{
                if (auto frame = segmentor.try_consume_once(); !frame.empty()) {
                    tile_frame = std::move(frame);
                    return true;
                }
                return false;
            };
        };
    }

    frame_indexed_builder queued_frame_builder(unsigned concurrency) {
        return [concurrency](std::pair<int, int> ordinal,
                             multi_buffer& initial_buffer,
                             multi_buffer&& tile_buffer)
            -> frame_consumer {
            static_assert(std::is_move_assignable<frame_segmentor>::value);
            static_assert(std::is_move_assignable<multi_buffer>::value);
            auto tile_decode_task = folly::Future<int64_t>::makeEmpty();
            {
                auto tile_context = std::make_unique<tile_media_context>();
                tile_context->segmentor = frame_segmentor{ core::split_buffer_sequence(initial_buffer,tile_buffer),concurrency };
                tile_context->tile_buffer = std::move(tile_buffer);
                tile_decode_task = folly::via(
                    session_executor.get(),
                    [ordinal, tile_context = std::move(tile_context)]{
                        auto decode_count = 0i64;
                        auto& texture_context = texture_at(ordinal.first, ordinal.second);
                        try {
                            while (true) {
                                for (auto& frame : tile_context->segmentor.try_consume()) {
                                    texture_context.frame_queue.enqueue(std::move(frame));
                                    decode_count++;
                                }
                            }
                        } catch (core::stream_drained_error) {
                            texture_context.frame_queue.enqueue(media::frame{ nullptr });
                        }
                        return decode_count;
                    });
            }
            return[tile_task = std::move(tile_decode_task)]{
                return !tile_task.isReady();
            };
        };
    }

    void DLLAPI unity::_nativeDashCreate(LPCSTR mpd_url) {
        manager = dash_manager::create_parsed(std::string{ mpd_url }, asio_concurrency);
    }

    BOOL DLLAPI _nativeDashGraphicInfo(INT & col, INT & row, INT & width, INT & height) {
        manager.wait();
        if (manager.hasValue()) {
            frame_grid = manager.value().grid_size();
            frame_scale = manager.value().scale_size();
            std::tie(col, row) = frame_grid;
            std::tie(width, height) = frame_scale;
            tile_width = frame_scale.first / frame_grid.first;
            tile_height = frame_scale.second / frame_grid.second;
            tile_textures.clear();
            if (!session_executor) {
                session_executor = core::make_pool_executor(col*row);
            }
            return true;
        }
        return false;
    }

    void DLLAPI _nativeDashSetTexture(INT x, INT y,
                                      HANDLE tex_y, HANDLE tex_u, HANDLE tex_v) {
        assert(manager.isReady());
        assert(manager.hasValue());
        auto& tile_texture = texture_at(x, y, true);
        tile_texture.x = x;
        tile_texture.y = y;
        tile_texture.texture[0] = static_cast<ID3D11Texture2D*>(tex_y);
        tile_texture.texture[1] = static_cast<ID3D11Texture2D*>(tex_u);
        tile_texture.texture[2] = static_cast<ID3D11Texture2D*>(tex_v);
    }

    void _nativeDashPrefetch() {
        assert(manager.isReady());
        assert(manager.hasValue());
        manager.value().register_represent_builder(queued_frame_builder(decoder_concurrency));
    }

    BOOL DLLAPI _nativeDashAvailable() {
        return manager.value().available();
    }

    BOOL DLLAPI _nativeDashTilePollUpdate(INT x, INT y) {
        //if (manager.value().poll_tile_consumed(x, y)) {
        //    static_assert(std::is_nothrow_move_assignable<media::frame>::value);
        //    tile_textures.at(tile_index(x, y)).frame = std::move(tile_frame);
        //    assert(tile_frame.operator->() == nullptr);
        //    return true;
        //}
        //return false;
        if (texture_at(x, y).frame_queue.empty()) {
            return false;
        }
        state::tile_col = x;
        state::tile_row = y;
        return true;
    }

    BOOL DLLAPI _nativeDashTileWaitUpdate(INT x, INT y) {
        return manager.value().wait_tile_consumed(x, y);
    }

    void _nativeGraphicSetTextures(HANDLE tex_y, HANDLE tex_u, HANDLE tex_v) {
        if constexpr (!::debug::enable_null_texture) {
            assert(tex_y != nullptr);
            assert(tex_u != nullptr);
            assert(tex_v != nullptr);
        }
        core::access(dll_graphic)->store_textures(tex_y, tex_u, tex_v);
    }

    void _nativeGraphicRelease() {
        if (dll_graphic) {
            dll_graphic->clean_up();
        }
    }

    namespace test
    {
        void _nativeMockGraphic() {
            assert(!dll_graphic);
            core::access(dll_graphic);
            assert(dll_graphic);
        }
    }
}

static void __stdcall OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType) {
    if (eventType == kUnityGfxDeviceEventInitialize) {
        core::verify(unity_graphics->GetRenderer() == kUnityGfxRendererD3D11);
        unity_time = 0;
        dll_graphic = std::make_shared<dll::graphic>();
        unity_device = kUnityGfxRendererD3D11;
    }
    if (dll_graphic != nullptr) {
        dll_graphic->process_event(eventType, unity_interface);
    }
    if (eventType == kUnityGfxDeviceEventShutdown) {
        unity_device = kUnityGfxRendererNull;
        dll_graphic = nullptr;
    }
}

EXTERN_C void DLLAPI __stdcall UnityPluginLoad(IUnityInterfaces* unityInterfaces) {
    unity_interface = unityInterfaces;
    unity_graphics = unity_interface->Get<IUnityGraphics>();
    unity_graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);
    OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
}

EXTERN_C void DLLAPI __stdcall UnityPluginUnload() {
    unity_graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
}

static void __stdcall OnRenderEvent(int eventID) {
    if (eventID > 0) {
        if constexpr (debug::enable_dump) {
            const auto r = (eventID - 1) / debug::pixel_dump_col;
            const auto c = (eventID - 1) % debug::pixel_dump_col;
            std::ofstream file{ fmt::format("F:/debug/ny_{}_{}.yuv",c,r),std::ios::binary | std::ios::app };
            file.write(reinterpret_cast<const char*>(tile_frame->data[0]), debug::pixel_dump_width * debug::pixel_dump_height);
            file.write(reinterpret_cast<const char*>(tile_frame->data[1]), debug::pixel_dump_width * debug::pixel_dump_height / 4);
            file.write(reinterpret_cast<const char*>(tile_frame->data[2]), debug::pixel_dump_width * debug::pixel_dump_height / 4);
        }
        assert(!std::empty(tile_textures));
        auto& tile_texture = state::texture_iterator->second;
        media::frame tile_frame{ nullptr };
        if (tile_texture.frame_queue.try_dequeue(tile_frame)) {
            if (tile_frame.empty()) {

            }
            return dll_graphic->update_textures(tile_frame, tile_texture.texture);
        }
        assert(false);
    } else {
        dll_graphic->update_textures(tile_frame);
    }
}

UnityRenderingEvent unity::_nativeGraphicGetRenderEventFunc() {
    return OnRenderEvent;
}