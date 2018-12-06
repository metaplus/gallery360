#pragma once

namespace unity
{
    // Exported c-linkage interface invoked as c# function for Unity extension. Familiar C# CamelCase naming style.
    extern "C" {

    void DLLAPI __stdcall _nativeLibraryInitialize();
    void DLLAPI __stdcall _nativeLibraryRelease();

    namespace test
    {
        void DLLAPI __stdcall _nativeMockGraphic();
        void DLLAPI __stdcall _nativeConfigConcurrency(UINT codec, UINT net = 8);
        void DLLAPI __stdcall _nativeConcurrencyValue(UINT& codec, UINT& net, UINT& executor);
        void DLLAPI __stdcall _nativeCoordinateState(INT& col, INT& row);
        LPSTR DLLAPI __stdcall _nativeTestString();
    }

    void DLLAPI __stdcall _nativeDashCreate(LPCSTR mpd_url);
    BOOL DLLAPI __stdcall _nativeDashGraphicInfo(INT& col, INT& row, INT& width, INT& height);
    void DLLAPI __stdcall _nativeDashCreateTileStream(INT col, INT row, INT index,
                                                      HANDLE tex_y, HANDLE tex_u, HANDLE tex_v);
    void DLLAPI __stdcall _nativeDashPrefetch();
    BOOL DLLAPI __stdcall _nativeDashAvailable();
    BOOL DLLAPI __stdcall _nativeDashTilePollUpdate(INT col, INT row, INT64 frame_index, INT64 batch_index);

    void DLLAPI __stdcall _nativeGraphicSetTextures(HANDLE tex_y, HANDLE tex_u, HANDLE tex_v, BOOL temp);
    HANDLE DLLAPI __stdcall _nativeGraphicCreateTextures(INT width, INT height, CHAR value);
    UnityRenderingEvent DLLAPI __stdcall _nativeGraphicGetRenderEventFunc();
    UnityRenderingEventAndData DLLAPI __stdcall _nativeGraphicGetUpdateCallback();

    }
}
