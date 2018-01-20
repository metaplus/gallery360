#pragma once
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
EXTERN void UNITYAPI PushAlphaTexture(HANDLE texY, HANDLE texU, HANDLE texV);
//EXTERN void UNITYAPI InfoResolution(INT& width,INT& height);
EXTERN void UNITYAPI InfoPlayParamsVideo(INT& width,INT& height);
EXTERN BOOL UNITYAPI IsDrainedVideo();
EXTERN void UNITYAPI Release();
//EXTERN INT UNITYAPI CreateTextures(HANDLE& y,HANDLE& u,HANDLE& v);
//EXTERN void UNITYAPI UnityPluginLoad(IUnityInterfaces* unityInterfaces);
//EXTERN void UNITYAPI UnityPluginUnload();
//EXTERN void OnRenderEvent(int eventID);
//EXTERN void OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType);
//EXTERN UnityRenderingEvent UNITYAPI GetRenderEventFunc();
/**
 * @note Cross tanslation unit functions. No exposed shared variable.
 */
namespace dll
{
    av::frame ExtractFrame();
    void MediaClear();
    void MediaRelease();
}