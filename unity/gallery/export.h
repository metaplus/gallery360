#pragma once

namespace unity
{
    // Exported c-linkage interface invoked as c# function for Unity extension. Familiar C# CamelCase naming style.
    EXTERN_C void DLLAPI _nativeLibraryInitialize();
    EXTERN_C void DLLAPI _nativeLibraryRelease();

    EXTERN_C void DLLAPI _nativeMediaCreate();
    EXTERN_C void DLLAPI _nativeMediaRelease();
    EXTERN_C UINT64 DLLAPI _nativeMediaSessionCreate(LPCSTR url);
    EXTERN_C INT64 DLLAPI _nativeMediaSessionCreateNetStream(LPCSTR url, INT row, INT column);
    EXTERN_C void DLLAPI _nativeMediaSessionRelease(UINT64 hashID);
    EXTERN_C void DLLAPI _nativeMediaSessionGetResolution(UINT64 hashID, INT& width, INT& height);
    EXTERN_C BOOL DLLAPI _nativeMediaSessionHasNextFrame(UINT64 hashID);

    namespace debug
    {
        EXTERN_C BOOL DLLAPI _nativeMediaSessionDropFrame(UINT64 hashID, INT64 count = 1);
    }

    EXTERN_C void DLLAPI _nativeGraphicSetTextures(HANDLE textureY, HANDLE textureU, HANDLE textureV);
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
