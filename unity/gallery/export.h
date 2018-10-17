#pragma once

namespace unity
{
    // Exported c-linkage interface invoked as c# function for Unity extension. Familiar C# CamelCase naming style.
    EXTERN_C void DLLAPI _nativeLibraryInitialize();
    EXTERN_C void DLLAPI _nativeLibraryRelease();

    EXTERN_C INT16 DLLAPI _nativeConfigureMedia(INT16 streams);
    EXTERN_C void DLLAPI _nativeConfigureNet(INT16 threads);

    EXTERN_C INT64 DLLAPI _nativeMediaSessionCreateFileReader(LPCSTR url);
    EXTERN_C INT64 DLLAPI _nativeMediaSessionCreateNetStream(LPCSTR url, INT row, INT column);
    EXTERN_C INT64 DLLAPI _nativeMediaSessionCreateDashMpd(LPCSTR url);
    EXTERN_C INT64 DLLAPI _nativeMediaSessionCreateDashStream(LPCSTR url, INT row, INT column, INT64 last_tile_index);
    EXTERN_C void DLLAPI _nativeMediaSessionRelease(INT64 id);
    EXTERN_C void DLLAPI _nativeMediaSessionGetResolution(INT64 id, INT& width, INT& height);
    EXTERN_C BOOL DLLAPI  _nativeMediaSessionHasNextFrame(INT64 id);

    namespace test
    {
        EXTERN_C BOOL DLLAPI _nativeMediaSessionDropFrame(INT64 id);
        EXTERN_C void DLLAPI _nativeMockGraphic();
    }

    EXTERN_C void DLLAPI _nativeConfigExecutor();
    EXTERN_C void DLLAPI _nativeDashCreate(LPCSTR mpd_url);
    EXTERN_C BOOL DLLAPI _nativeDashGraphicInfo(INT& col, INT& row, INT& width, INT& height);
    EXTERN_C void DLLAPI _nativeDashSetTexture(INT x, INT y, HANDLE textureY, HANDLE textureU, HANDLE textureV);
    EXTERN_C void DLLAPI _nativeDashPrefetch();
    EXTERN_C BOOL DLLAPI _nativeDashAvailable();
    EXTERN_C BOOL DLLAPI _nativeDashTilePollUpdate(INT x, INT y);
    EXTERN_C BOOL DLLAPI _nativeDashTileWaitUpdate(INT x, INT y);

    EXTERN_C void DLLAPI _nativeGraphicSetTextures(HANDLE textureY, HANDLE textureU, HANDLE textureV);
    EXTERN_C BOOL DLLAPI _nativeGraphicUpdateTextures(INT64 id, HANDLE textureY, HANDLE textureU, HANDLE textureV);
    EXTERN_C void DLLAPI _nativeGraphicRelease();
    EXTERN_C UnityRenderingEvent DLLAPI __stdcall _nativeGraphicGetRenderEventFunc();

#ifdef GALLERY_USE_LEGACY
    EXTERN_C BOOL DLLAPI global_create();
    EXTERN_C void DLLAPI global_release();
    EXTERN_C BOOL DLLAPI store_media_url(LPCSTR url);
    EXTERN_C void DLLAPI store_time(FLOAT t);
    EXTERN_C void DLLAPI store_alpha_texture(HANDLE texY, HANDLE texU, HANDLE texV);
    EXTERN_C UINT32 DLLAPI store_vr_frame_timing(HANDLE vr_timing);
    EXTERN_C UINT32 DLLAPI store_vr_cumulative_status(HANDLE vr_status);
    EXTERN_C void DLLAPI load_video_params(INT& width, INT& height);
    EXTERN_C BOOL DLLAPI is_video_available();
#endif  // GALLERY_USE_LEGACY
}
