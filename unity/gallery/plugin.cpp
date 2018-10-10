#include "stdafx.h"
#include "export.h"
#include "network/component.h"
#include "multimedia/component.h"
#include "multimedia/pch.h"
#include "graphic.h"

using net::component::dash_manager;
using net::component::frame_consumer;
using net::component::frame_builder;
using net::component::ordinal;
using media::component::frame_segmentor;
using media::component::pixel_array;
using media::component::pixel_consume;
using boost::beast::multi_buffer;

struct texture_bundle;

std::shared_ptr<folly::Executor> executor;
folly::Future<dash_manager> manager = folly::Future<dash_manager>::makeEmpty();
pixel_consume pixel_update;
pixel_array pixels;
media::frame tile_frame{ nullptr };

const auto asio_concurrency = std::thread::hardware_concurrency() / 2;
const auto executor_concurrency = std::thread::hardware_concurrency();
const auto decoder_concurrency = 4u;

float unity_time = 0;
std::shared_ptr<dll::graphic> dll_graphic;
UnityGfxRenderer unity_device = kUnityGfxRendererNull;
IUnityInterfaces* unity_interface = nullptr;
IUnityGraphics* unity_graphics = nullptr;
std::map<int, texture_bundle> tile_textures;

struct texture_bundle
{
    std::array<ID3D11Texture2D*, 3> texture;
    int x = 0;
    int y = 0;
};

namespace unity
{
    void DLLAPI unity::_nativeConfigExecutor() {
        if (!executor) {
            executor = core::set_cpu_executor(executor_concurrency, "GalleryPool");
        }
    }

    frame_builder create_frame_builder(unsigned concurrency = decoder_concurrency) {
        return [concurrency](multi_buffer& head_buffer, multi_buffer&& tail_buffer)-> frame_consumer {
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

    void DLLAPI unity::_nativeDashCreate(LPCSTR mpd_url) {
        manager = dash_manager::async_create_parsed(std::string{ mpd_url }, asio_concurrency);
    }

    std::pair<int, int> frame_grid;
    std::pair<int, int> frame_scale;
    int tile_width = 0;
    int tile_height = 0;

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
            return true;
        }
        return false;
    }

    void DLLAPI _nativeDashSetTexture(INT x, INT y,
                                      HANDLE tex_y, HANDLE tex_u, HANDLE tex_v) {
        assert(manager.isReady());
        assert(manager.hasValue());
        assert(tex_y != nullptr);
        assert(tex_u != nullptr);
        assert(tex_v != nullptr);
        auto index = x + y * frame_grid.first + 1;
        assert(!tile_textures.count(index));
        tile_textures[index].x = x;
        tile_textures[index].y = y;
        tile_textures[index].texture[0] = static_cast<ID3D11Texture2D*>(tex_y);
        tile_textures[index].texture[1] = static_cast<ID3D11Texture2D*>(tex_u);
        tile_textures[index].texture[2] = static_cast<ID3D11Texture2D*>(tex_v);
    }

    void _nativeDashPrefetch() {
        assert(manager.isReady());
        assert(manager.hasValue());
        //assert(pixel_update);
        manager.value().register_represent_builder(create_frame_builder(decoder_concurrency));
    }

    BOOL DLLAPI _nativeDashAvailable() {
        return manager.value().available();
    }

    BOOL DLLAPI _nativeDashTilePollUpdate(INT x, INT y) {
        return manager.value().poll_tile_consumed(x, y);
    }

    BOOL DLLAPI _nativeDashTileWaitUpdate(INT x, INT y) {
        return manager.value().wait_tile_consumed(x, y);
    }

    void _nativeGraphicSetTextures(HANDLE textureY, HANDLE textureU, HANDLE textureV) {
        //assert(textureY != nullptr);
        //assert(textureU != nullptr);
        //assert(textureV != nullptr);
        core::access(dll_graphic)->store_textures(textureY, textureU, textureV);
    }

    void _nativeGraphicRelease() {
        if (dll_graphic) {
            dll_graphic->clean_up();
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

#if _DEBUG
#define DEBUG_PIXEL_DUMP_COL 0
#define DEBUG_PIXEL_DUMP_WIDTH 1280
#define DEBUG_PIXEL_DUMP_HEIGHT 640
#endif

static void __stdcall OnRenderEvent(int eventID) {
    if (eventID > 0) {
    #if DEBUG_PIXEL_DUMP_COL > 0
        const auto r = (eventID - 1) / DEBUG_PIXEL_DUMP_COL;
        const auto c = (eventID - 1) % DEBUG_PIXEL_DUMP_COL;
        std::ofstream fout{ fmt::format("F:/debug/ny_{}_{}.yuv",c,r),std::ios::out | std::ios::binary | std::ios::app };
        fout.write((const char*)tile_frame->data[0], DEBUG_PIXEL_DUMP_WIDTH * DEBUG_PIXEL_DUMP_HEIGHT);
        fout.write((const char*)tile_frame->data[1], DEBUG_PIXEL_DUMP_WIDTH * DEBUG_PIXEL_DUMP_HEIGHT / 4);
        fout.write((const char*)tile_frame->data[2], DEBUG_PIXEL_DUMP_WIDTH * DEBUG_PIXEL_DUMP_HEIGHT / 4);
    #endif
        assert(!std::empty(tile_textures));
        dll_graphic->update_textures(tile_frame, tile_textures.at(eventID).texture);
    } else {
        dll_graphic->update_textures(tile_frame);
    }
}

UnityRenderingEvent unity::_nativeGraphicGetRenderEventFunc() {
    return OnRenderEvent;
}