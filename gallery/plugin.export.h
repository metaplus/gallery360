#pragma once

namespace unity
{
    //  Exported c-linkage interface invoked as c# function for Unity extensions. 
    //  Familiar C# CamelCase naming style.
    extern "C" {

    BOOL DLL_EXPORT __stdcall _nativeLibraryConfigLoad(LPCSTR mpd_url);
    void DLL_EXPORT __stdcall _nativeLibraryInitialize();
    void DLL_EXPORT __stdcall _nativeLibraryRelease();
    BOOL DLL_EXPORT __stdcall _nativeLibraryTraceEvent(LPCSTR instance, LPCSTR event);

    namespace test
    {
        void DLL_EXPORT __stdcall _nativeTestGraphicCreate();
        void DLL_EXPORT __stdcall _nativeTestConcurrencyStore(UINT codec, UINT net = 8);
        void DLL_EXPORT __stdcall _nativeTestConcurrencyLoad(UINT& codec, UINT& net, UINT& executor);
        LPSTR DLL_EXPORT __stdcall _nativeTestString();
    }

    LPSTR DLL_EXPORT __stdcall _nativeDashCreate();
    BOOL DLL_EXPORT __stdcall _nativeDashGraphicInfo(INT& col, INT& row, INT& width, INT& height);
    HANDLE DLL_EXPORT __stdcall _nativeDashCreateTileStream(INT col, INT row, INT index,
                                                            HANDLE tex_y, HANDLE tex_u, HANDLE tex_v);
    void DLL_EXPORT __stdcall _nativeDashPrefetch();
    BOOL DLL_EXPORT __stdcall _nativeDashAvailable();
    BOOL DLL_EXPORT __stdcall _nativeDashTilePollUpdate(INT col, INT row, INT64 frame_index, INT64 batch_index);
    BOOL DLL_EXPORT __stdcall _nativeDashTilePtrPollUpdate(HANDLE instance, INT64 frame_index, INT64 batch_index);
    void DLL_EXPORT __stdcall _nativeDashTileFieldOfView(INT col, INT row);

    void DLL_EXPORT __stdcall _nativeGraphicSetTextures(HANDLE tex_y, HANDLE tex_u, HANDLE tex_v, BOOL temp);
    HANDLE DLL_EXPORT __stdcall _nativeGraphicCreateTextures(INT width, INT height, CHAR value);
    UnityRenderingEvent DLL_EXPORT __stdcall _nativeGraphicGetRenderCallback();
    UnityRenderingEventAndData DLL_EXPORT __stdcall _nativeGraphicGetUpdateCallback();

    void DLL_EXPORT __stdcall UnityPluginLoad(IUnityInterfaces* unityInterfaces);
    void DLL_EXPORT __stdcall UnityPluginUnload();
    }
}
