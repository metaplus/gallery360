#include "stdafx.h"
#include "export.h"
#include "network/component.h"
#include "multimedia/component.h"
#include "graphic.h"

using net::component::dash_manager;
using net::component::frame_consumer;
using net::component::frame_builder;
using net::component::ordinal;
using media::component::frame_segmentor;
using media::component::pixel_array;
using media::component::pixel_consume;
using boost::beast::multi_buffer;

std::shared_ptr<folly::Executor> executor;
folly::Future<dash_manager> manager = folly::Future<dash_manager>::makeEmpty();
pixel_consume pixel_update;

const auto asio_concurrency = std::thread::hardware_concurrency() / 2;
const auto executor_concurrency = std::thread::hardware_concurrency();
const auto decoder_concurrency = 2u;

float unity_time = 0;
std::shared_ptr<dll::graphic> dll_graphic;
UnityGfxRenderer unity_device = kUnityGfxRendererNull;
IUnityInterfaces* unity_interface = nullptr;
IUnityGraphics* unity_graphics = nullptr;

namespace unity
{
    void DLLAPI unity::_nativeConfigExecutor() {
        if (!executor) {
            executor = core::set_cpu_executor(executor_concurrency, "GalleryPool");
        }
    }

    frame_builder create_frame_builder(pixel_consume& consume,
                                       unsigned concurrency = decoder_concurrency) {
        return [&consume, concurrency](multi_buffer& head_buffer, multi_buffer&& tail_buffer)-> frame_consumer {
            auto segmentor = folly::makeMoveWrapper(
                frame_segmentor{ core::split_buffer_sequence(head_buffer,tail_buffer),concurrency });
            auto tail_buffer_wrapper = folly::makeMoveWrapper(tail_buffer);
            auto decode = folly::makeMoveWrapper(segmentor->defer_consume_once(consume));
            return [segmentor, decode, tail_buffer_wrapper, &consume]() mutable {
                const auto result = std::move(*decode).get();
                *decode = segmentor->defer_consume_once(consume);
                return result;
            };
        };
    }

    void DLLAPI unity::_nativeDashCreate(LPCSTR mpd_url) {
        manager = dash_manager::async_create_parsed(std::string{ mpd_url }, asio_concurrency);
    }

    BOOL DLLAPI _nativeDashGraphicInfo(INT & col, INT & row, INT & width, INT & height) {
        manager.wait();
        if (manager.hasValue()) {
            std::tie(col, row) = manager.value().grid_size();
            std::tie(width, height) = manager.value().scale_size();
            return true;
        }
        return false;
    }

    void _nativeDashPrefetch() {
        assert(manager.isReady());
        assert(manager.hasValue());
        assert(pixel_update);
        manager.value().register_represent_builder(create_frame_builder(pixel_update, decoder_concurrency));
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

    BOOL _nativeGraphicUpdateTextures(INT64 id, HANDLE textureY, HANDLE textureU, HANDLE textureV) {
        //if (auto frame = dll::media_module::try_take_decoded_frame(id); !frame.empty()) {
            //dll::graphic_module::update_textures(frame, std::array<ID3D11Texture2D*, 3>{
            //    static_cast<ID3D11Texture2D*>(textureY), static_cast<ID3D11Texture2D*>(textureU), static_cast<ID3D11Texture2D*>(textureV) });
        //    return true;
        //}
        return false;
    }

    void _nativeGraphicSetTextures(HANDLE textureY, HANDLE textureU, HANDLE textureV) {
        //assert(textureY != nullptr);
        //assert(textureU != nullptr);
        //assert(textureV != nullptr);
        core::access(dll_graphic)->store_textures(textureY, textureU, textureV);
        pixel_update = [](pixel_array pixel_array) {
            assert(pixel_array[0] != nullptr);
            assert(pixel_array[1] != nullptr);
            assert(pixel_array[2] != nullptr);
            dll_graphic->update_textures(pixel_array);
        };
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

static void __stdcall OnRenderEvent(int eventID) {
    //if (auto frame = dll::media_module::try_get_decoded_frame(); frame.has_value())
    //if (auto frame = dll::media_module::try_take_decoded_frame(0); !frame.empty())
    //    dll_graphic->update_textures(frame);
}

UnityRenderingEvent unity::_nativeGraphicGetRenderEventFunc() {
    return OnRenderEvent;
}