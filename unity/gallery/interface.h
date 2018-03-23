#pragma once

/**
 * @note Exported functions called by Unity engine. C# CamelCase naming style.
 */
namespace unity
{
    EXTERN BOOL DLLAPI global_create();
    EXTERN void DLLAPI global_release();
    EXTERN BOOL DLLAPI store_media_url(LPCSTR url);
    EXTERN void DLLAPI store_time(FLOAT t);
    EXTERN void DLLAPI store_alpha_texture(HANDLE texY, HANDLE texU, HANDLE texV);
    EXTERN UINT32 DLLAPI store_vr_frame_timing(HANDLE vr_timing);
    EXTERN UINT32 DLLAPI store_vr_cumulative_status(HANDLE vr_status);
    EXTERN void DLLAPI load_video_params(INT& width, INT& height);
    EXTERN BOOL DLLAPI is_video_available();
    EXTERN_C UINT DLLAPI media_create();
}

namespace dll
{
    DLLAPI void timer_startup();
    DLLAPI std::chrono::high_resolution_clock::duration timer_elapsed();
    DLLAPI void media_prepare();
    DLLAPI void media_create();
    DLLAPI void media_release();
    DLLAPI std::optional<av::frame> media_retrieve_frame();
    DLLAPI std::pair<std::string, av::codec_context> media_retrieve_format();
    DLLAPI void interprocess_create();
    DLLAPI void interprocess_release();
    DLLAPI void interprocess_async_send(ipc::message message);
    DLLAPI void interprocess_send(ipc::message message);
    DLLAPI void graphics_release();
}