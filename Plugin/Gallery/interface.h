#pragma once
/**
 * @note Exported functions called by Unity engine. C# CamelCase naming style.
 */

namespace unity
{
    EXTERN BOOL UNITYAPI GlobalCreate();
    EXTERN void UNITYAPI GlobalRelease();
    EXTERN BOOL UNITYAPI StoreMediaUrl(LPCSTR url);
    EXTERN void UNITYAPI StoreTime(FLOAT t);
    EXTERN void UNITYAPI StoreAlphaTexture(HANDLE texY, HANDLE texU, HANDLE texV);
    EXTERN UINT32 UNITYAPI StoreVrFrameTiming(HANDLE vr_timing);
    EXTERN UINT32 UNITYAPI StoreVrCumulativeStatus(HANDLE vr_status);
    EXTERN void UNITYAPI LoadVideoParams(INT& width, INT& height);
    EXTERN BOOL UNITYAPI IsVideoAvailable();
}

namespace dll
{
    DLLAPI void timer_startup();
    DLLAPI std::chrono::high_resolution_clock::duration timer_elapsed();
    DLLAPI void media_prepare();
    DLLAPI void media_create();
    DLLAPI void media_release();
    DLLAPI std::optional<av::frame> media_extract_frame();
    DLLAPI std::string media_wait_decoding_start();
    DLLAPI void interprocess_create();
    DLLAPI void interprocess_release();
    DLLAPI void interprocess_async_send(ipc::message message);
    DLLAPI void interprocess_send(ipc::message message);
    DLLAPI void graphics_release();
}
