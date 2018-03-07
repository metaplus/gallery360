#pragma once
/**
 * @note Exported functions called by Unity engine. C# CamelCase naming style.
 */
EXTERN BOOL UNITYAPI GlobalCreate();
EXTERN void UNITYAPI GlobalRelease();
EXTERN BOOL UNITYAPI StoreMediaUrl(LPCSTR url);
EXTERN void UNITYAPI StoreTime(FLOAT t);
EXTERN void UNITYAPI StoreAlphaTexture(HANDLE texY, HANDLE texU, HANDLE texV);
EXTERN UINT32 UNITYAPI StoreVrFrameTiming(HANDLE vr_timing);
EXTERN UINT32 UNITYAPI StoreVrCumulativeStatus(HANDLE vr_status);
EXTERN void UNITYAPI LoadVideoParams(INT& width,INT& height);
EXTERN BOOL UNITYAPI IsVideoDrained();
namespace dll
{
    DLLAPI void timer_startup();
    DLLAPI std::chrono::high_resolution_clock::duration timer_elapsed();
    std::optional<av::frame> media_extract_frame();
    void media_create();
    void media_clear();
    void media_release();
    void ipc_create();
    void ipc_release();
    void ipc_async_send(ipc::message message);                         
    std::pair<std::future<ipc::message>, size_t> ipc_async_receive();
    ipc::message ipc_receive();
    namespace helper
    {
        
    }
}
static_assert(std::is_same_v<int, INT>);
static_assert(std::is_same_v<float, FLOAT>);
