#pragma once

namespace unity
{
    // Exported c-linkage interface invoked as c# function for Unity extension. Familiar C# CamelCase naming style.
    extern "C" {

    BOOL DLL_EXPORT __stdcall _nativeLoadEnvConfig();
    void DLL_EXPORT __stdcall _nativeLibraryInitialize();
    void DLL_EXPORT __stdcall _nativeLibraryRelease();

    namespace test
    {
        void DLL_EXPORT __stdcall _nativeMockGraphic();
        void DLL_EXPORT __stdcall _nativeConfigConcurrency(UINT codec, UINT net = 8);
        void DLL_EXPORT __stdcall _nativeConcurrencyValue(UINT& codec, UINT& net, UINT& executor);
        LPSTR DLL_EXPORT __stdcall _nativeTestString();
    }

    LPSTR DLL_EXPORT __stdcall _nativeDashCreate();
    BOOL DLL_EXPORT __stdcall _nativeDashGraphicInfo(INT& col, INT& row, INT& width, INT& height);
    void DLL_EXPORT __stdcall _nativeDashCreateTileStream(INT col, INT row, INT index,
                                                          HANDLE tex_y, HANDLE tex_u, HANDLE tex_v);
    void DLL_EXPORT __stdcall _nativeDashPrefetch();
    BOOL DLL_EXPORT __stdcall _nativeDashAvailable();
    BOOL DLL_EXPORT __stdcall _nativeDashTilePollUpdate(INT col, INT row, INT64 frame_index, INT64 batch_index);
    INT DLL_EXPORT __stdcall _nativeDashTilePollUpdateFrame(INT64 frame_index, INT64 batch_index);
    void DLL_EXPORT __stdcall _nativeDashTileFieldOfView(INT col, INT row);

    void DLL_EXPORT __stdcall _nativeGraphicSetTextures(HANDLE tex_y, HANDLE tex_u, HANDLE tex_v, BOOL temp);
    HANDLE DLL_EXPORT __stdcall _nativeGraphicCreateTextures(INT width, INT height, CHAR value);
    UnityRenderingEvent DLL_EXPORT __stdcall _nativeGraphicGetRenderCallback();
    UnityRenderingEventAndData DLL_EXPORT __stdcall _nativeGraphicGetUpdateCallback();

    }
}
