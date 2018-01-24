#include "stdafx.h"
#include "interface.h"

namespace
{
    float unity_time = 0;
    //render handle{}; 
    std::unique_ptr<render> handle = nullptr;
    UnityGfxRenderer unity_device = kUnityGfxRendererNull;
    IUnityInterfaces* unity_interface = nullptr;
    IUnityGraphics* unity_graphics = nullptr;
}
void StoreTime(FLOAT t)
{
    unity_time = t;
}
void StoreAlphaTexture(HANDLE texY, HANDLE texU, HANDLE texV)
{
    core::verify(texY != nullptr, texU != nullptr, texV != nullptr);
    handle->store_textures(texY, texU, texV);
}
static void __stdcall OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
    if (eventType == kUnityGfxDeviceEventInitialize)
    {
        core::verify(unity_graphics->GetRenderer() == kUnityGfxRendererD3D11);
        unity_time = 0;
        handle = std::make_unique<render>();
        unity_device = kUnityGfxRendererD3D11;
    }
    if (handle != nullptr)
    {
        handle->process_event(eventType, unity_interface);
    }
    if (eventType == kUnityGfxDeviceEventShutdown)
    {
        unity_device = kUnityGfxRendererNull;
        handle.reset();
    }
}
EXTERN void UNITYAPI UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
    unity_interface = unityInterfaces;
    unity_graphics = unity_interface->Get<IUnityGraphics>();
    unity_graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);
    OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
    //GlobalCreate();
}
EXTERN void UNITYAPI UnityPluginUnload()
{
    //GlobalRelease();
    unity_graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
}
static void __stdcall OnRenderEvent(int eventID)
{
    core::verify(unity_graphics != nullptr);
    auto frame = dll::extract_frame();
    handle->update_textures(frame);
}
EXTERN UnityRenderingEvent UNITYAPI GetRenderEventFunc()
{
    return OnRenderEvent;
}
