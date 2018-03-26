using System;
using System.Runtime.InteropServices;
using JetBrains.Annotations;
using Valve.VR;

namespace Gallery
{
    public partial class MediaPlayer
    {
        [UsedImplicitly]
        private class Native
        {
            [UsedImplicitly]
            public class Media
            {
                [DllImport("gallery", EntryPoint = "_nativeMediaCreate", CallingConvention = CallingConvention.Cdecl)]
                internal static extern void Create();

                [DllImport("gallery", EntryPoint = "_nativeMediaRelease", CallingConvention = CallingConvention.Cdecl)]
                internal static extern void Release();

                [DllImport("gallery", EntryPoint = "_nativeMediaSessionCreate", CallingConvention = CallingConvention.Cdecl)]
                internal static extern ulong CreateSession([In, MarshalAs(UnmanagedType.LPStr)]string url);

                [DllImport("gallery", EntryPoint = "_nativeMediaSessionPause", CallingConvention = CallingConvention.Cdecl)]
                internal static extern void PauseSession(ulong hashId);

                [DllImport("gallery", EntryPoint = "_nativeMediaSessionRelease", CallingConvention = CallingConvention.Cdecl)]
                internal static extern void ReleaseSession(ulong hashId);

                [DllImport("gallery", EntryPoint = "_nativeMediaSessionGetResolution", CallingConvention = CallingConvention.Cdecl)]
                internal static extern void GetSessionResolution(ulong hashId, ref int width, ref int height);

                [DllImport("gallery", EntryPoint = "_nativeMediaSessionHasNextFrame", CallingConvention = CallingConvention.Cdecl)]
                internal static extern bool IsSessionHasNextFrame(ulong hashId);
            }

            [UsedImplicitly]
            public class Graphic
            {
                private enum Event { RenderAnyFrame = 0, StopAndClear }
                [DllImport("gallery", EntryPoint = "_nativeGraphicSetTextures", CallingConvention = CallingConvention.Cdecl)]
                internal static extern void SetAlphaTexture(IntPtr texY, IntPtr texU, IntPtr texV);

                [DllImport("gallery", EntryPoint = "_nativeGraphicRelease", CallingConvention = CallingConvention.Cdecl)]
                internal static extern void Release();

                [DllImport("gallery", EntryPoint = "_nativeGraphicGetRenderEventFunc", CallingConvention = CallingConvention.StdCall)]
                internal static extern IntPtr GetRenderEventFunc();
            }

#if GALLERY_USE_LEGACY
        [DllImport("gallery", EntryPoint = "global_create", CallingConvention = CallingConvention.Cdecl)]
        internal static extern bool GlobalCreate();
        [DllImport("gallery", EntryPoint = "global_release", CallingConvention = CallingConvention.Cdecl)]
        internal static extern void GlobalRelease();
        [DllImport("gallery", EntryPoint = "store_time", CallingConvention = CallingConvention.Cdecl)]
        internal static extern void StoreTime(float t);
        [DllImport("gallery", EntryPoint = "store_alpha_texture", CallingConvention = CallingConvention.Cdecl)]
        internal static extern void StoreAlphaTexture(IntPtr texY, IntPtr texU, IntPtr texV);
        [DllImport("gallery", EntryPoint = "store_media_url", CallingConvention = CallingConvention.Cdecl)]
        internal static extern bool StoreMediaUrl([In, MarshalAs(UnmanagedType.LPStr)]string url);
        [DllImport("gallery", EntryPoint = "load_video_params", CallingConvention = CallingConvention.Cdecl)]
        internal static extern void LoadVideoParams(ref int width, ref int height);
        [DllImport("gallery", EntryPoint = "is_video_available", CallingConvention = CallingConvention.Cdecl)]
        internal static extern bool IsVideoAvailable();
        [DllImport("gallery", EntryPoint = "store_vr_frame_timing", CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint StoreVrFrameTiming(ref Compositor_FrameTiming vrTiming);
        [DllImport("gallery", EntryPoint = "store_vr_cumulative_status", CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint StoreVrCumulativeStatus(ref Compositor_CumulativeStats vrStats);
#endif
        }

        #region Index Definition

        public class Spatial
        {
            public class Index
            {
                private uint Vertical { get; set; }
                private uint Horizontal { get; set; }

                public Index(uint vertical = 0, uint horizontal = 0)
                {
                    Vertical = vertical;
                    Horizontal = horizontal;
                }
            }
        }

        #endregion
    }
}
