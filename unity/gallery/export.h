#pragma once

namespace unity
{
    // Exported c-linkage interface invoked as c# function for Unity extension. Familiar C# CamelCase naming style.
    EXTERN_C void DLLAPI _nativeLibraryInitialize();
    EXTERN_C void DLLAPI _nativeLibraryRelease();

    namespace test
    {
        EXTERN_C void DLLAPI _nativeMockGraphic();
        EXTERN_C void DLLAPI _nativeConfigConcurrency(UINT codec, UINT net = 8);
        EXTERN_C void DLLAPI _nativeConcurrencyValue(UINT& codec, UINT& net, UINT& executor);
        EXTERN_C void DLLAPI _nativeCoordinateState(INT& col, INT& row);
        EXTERN_C void DLLAPI _nativeTestString(LPSTR& str);
    }

    EXTERN_C void DLLAPI _nativeDashCreate(LPCSTR mpd_url);
    EXTERN_C BOOL DLLAPI _nativeDashGraphicInfo(INT& col, INT& row, INT& width, INT& height);
    EXTERN_C void DLLAPI _nativeDashSetTexture(INT col, INT row, INT index,
                                               HANDLE tex_y, HANDLE tex_u, HANDLE tex_v);
    EXTERN_C void DLLAPI _nativeDashPrefetch();
    EXTERN_C BOOL DLLAPI _nativeDashAvailable();
    EXTERN_C BOOL DLLAPI _nativeDashTilePollUpdate(INT col, INT row);
    EXTERN_C BOOL DLLAPI _nativeDashTileWaitUpdate(INT col, INT row);

    EXTERN_C void DLLAPI _nativeGraphicSetTextures(HANDLE tex_y, HANDLE tex_u, HANDLE tex_v, BOOL temp);

    EXTERN_C HANDLE DLLAPI _nativeGraphicCreateTextures(INT width, INT height);
    EXTERN_C BOOL DLLAPI _nativeGraphicUpdateTextures(INT64 id, HANDLE textureY, HANDLE textureU, HANDLE textureV);
    EXTERN_C void DLLAPI _nativeGraphicRelease();
    EXTERN_C UnityRenderingEvent DLLAPI __stdcall _nativeGraphicGetRenderEventFunc();
}
