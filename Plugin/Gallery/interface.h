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
    DLLAPI std::optional<av::frame> media_extract_frame();
    DLLAPI void media_create();
    DLLAPI void media_release();
    DLLAPI void interprocess_create();
    DLLAPI void interprocess_release();
    DLLAPI void interprocess_async_send(ipc::message message);                         
    DLLAPI std::pair<std::future<ipc::message>, size_t> interprocess_async_receive();
    namespace helper
    {
        class interprocess
        {
            std::chrono::high_resolution_clock::duration duration_;
        public:
            interprocess() : duration_(dll::timer_elapsed()) {}
            void send() && {}
        };
    }
}
static_assert(std::is_same_v<int, INT>);
static_assert(std::is_same_v<float, FLOAT>);
