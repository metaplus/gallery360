#pragma once
#if defined UNITYAPI || defined EXTERN
#error naming collide
#endif
#ifdef GALLERY_EXPORTS
#define UNITYAPI  __declspec(dllexport) __stdcall
#else
#define UNITYAPI  __declspec(dllimport) __stdcall
#endif
#ifdef __cplusplus
#define EXTERN extern "C"
#else 
#define EXTERN extern 
#endif
/**
 * @note Exported functions called by Unity engine. C# naming style.
 */
EXTERN BOOL UNITYAPI LaunchModules();
EXTERN BOOL UNITYAPI ParseMedia(LPCSTR url);
EXTERN void UNITYAPI StoreAlphaTexture(HANDLE texY, HANDLE texU, HANDLE texV);
EXTERN void UNITYAPI LoadParamsVideo(INT& width,INT& height);
EXTERN BOOL UNITYAPI IsDrainedVideo();
EXTERN void UNITYAPI Release();
//EXTERN INT UNITYAPI CreateTextures(HANDLE& y,HANDLE& u,HANDLE& v);
//EXTERN void UNITYAPI UnityPluginLoad(IUnityInterfaces* unityInterfaces);
//EXTERN void UNITYAPI UnityPluginUnload();
//EXTERN void OnRenderEvent(int eventID);
//EXTERN void OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType);
//EXTERN UnityRenderingEvent UNITYAPI GetRenderEventFunc();

namespace dll
{
    av::frame ExtractFrame();
    void MediaClear();
    void MediaRelease();
}