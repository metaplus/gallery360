using System;
using System.Runtime.InteropServices;
using JetBrains.Annotations;

namespace Assets.Gallery.Scripts
{
    public class Native
    {
        [DllImport("gallery", EntryPoint = "_nativeLibraryInitialize", CallingConvention = CallingConvention.StdCall)]
        internal static extern void Initialize();

        [DllImport("gallery", EntryPoint = "_nativeLibraryRelease", CallingConvention = CallingConvention.StdCall)]
        internal static extern void Release();

        [DllImport("gallery", EntryPoint = "_nativeLibraryConfigLoad", CallingConvention = CallingConvention.StdCall)]
        internal static extern bool LoadEnvConfig();

        [DllImport("gallery", EntryPoint = "_nativeLibraryConfigEntry", CallingConvention = CallingConvention.StdCall)]
        [return: MarshalAs(UnmanagedType.LPStr)]
        internal static extern string LoadEnvConfigEntry(
            [MarshalAs(UnmanagedType.LPStr)] string entryPath);

        [DllImport("gallery", EntryPoint = "_nativeLibraryTraceEvent", CallingConvention = CallingConvention.StdCall)]
        internal static extern bool TraceEvent(
            [MarshalAs(UnmanagedType.LPStr)] string instance,
            [MarshalAs(UnmanagedType.LPStr)] string eventContent);

        [DllImport("gallery", EntryPoint = "_nativeLibraryTraceMessage", CallingConvention = CallingConvention.StdCall)]
        internal static extern bool TraceMessage(
            [MarshalAs(UnmanagedType.LPStr)] string message);

        [UsedImplicitly]
        public class Graphic
        {
            [DllImport("gallery", EntryPoint = "_nativeGraphicSetTextures", CallingConvention = CallingConvention.StdCall)]
            internal static extern void SetAlphaTexture(IntPtr texY, IntPtr texU, IntPtr texV, bool temp);

            [DllImport("gallery", EntryPoint = "_nativeGraphicCreateTextures", CallingConvention = CallingConvention.StdCall)]
            internal static extern IntPtr CreateAlphaTexture(int width, int height, sbyte value);

            [DllImport("gallery", EntryPoint = "_nativeGraphicGetRenderCallback", CallingConvention = CallingConvention.StdCall)]
            internal static extern IntPtr GetRenderCallback();

            [DllImport("gallery", EntryPoint = "_nativeGraphicGetUpdateCallback", CallingConvention = CallingConvention.StdCall)]
            internal static extern IntPtr GetTextureUpdateCallback();
        }

        [UsedImplicitly]
        public class Dash
        {
            [DllImport("gallery", EntryPoint = "_nativeDashCreate", CallingConvention = CallingConvention.StdCall)]
            [return: MarshalAs(UnmanagedType.LPStr)]
            internal static extern string Create();

            [DllImport("gallery", EntryPoint = "_nativeDashGraphicInfo", CallingConvention = CallingConvention.StdCall)]
            internal static extern bool GraphicInfo(ref int col, ref int row, ref int width, ref int height);

            [DllImport("gallery", EntryPoint = "_nativeDashPrefetch", CallingConvention = CallingConvention.StdCall)]
            internal static extern void Prefetch();

            [DllImport("gallery", EntryPoint = "_nativeDashTilePollUpdate", CallingConvention = CallingConvention.StdCall)]
            internal static extern int PollTileUpdate(int x, int y, long frameIndex, long batchIndex);

            [DllImport("gallery", EntryPoint = "_nativeDashTilePtrPollUpdate", CallingConvention = CallingConvention.StdCall)]
            internal static extern int PollTilePtrUpdate(IntPtr instance, long frameIndex, long batchIndex);

            [DllImport("gallery", EntryPoint = "_nativeDashCreateTileStream", CallingConvention = CallingConvention.StdCall)]
            internal static extern IntPtr CreateTileStream(int col, int row, int index,
                IntPtr texY, IntPtr texU, IntPtr texV);

            [DllImport("gallery", EntryPoint = "_nativeDashTileFieldOfView", CallingConvention = CallingConvention.StdCall)]
            internal static extern bool NotifyFieldOfView(int col, int row);
        }
    }
}
