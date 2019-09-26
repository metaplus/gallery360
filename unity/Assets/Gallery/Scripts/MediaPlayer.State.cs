using System;
using System.Collections.Generic;
using System.Linq;
using JetBrains.Annotations;
using UnityEngine.Assertions;

#if UNITY_EDITOR
#endif

namespace Assets.Gallery.Scripts
{
    public partial class MediaPlayer
    {
        [UsedImplicitly]
        private class RenderState
        {
            public static bool FrameReadyWaiting { get; set; } = false;
            public static long FrameUpdateReadyIndex { get; set; } = 0;

            public static long FrameUpdateExecuteIndex { get; private set; } = 0;

            public static long FrameDecodeIndex { get; set; } = 0;

            public static long FrameLastIndex { get; set; } = 1;
            public static float FrameLastTime { get; set; } = 0f;
            public static long DecodeBatchIndex { get; set; } = 1;
            public static long JitterCount { get; private set; } = 0;
            public static bool FrameUpdateComplete { get; set; }
            public static float FrameUpdateExpectTime { get; set; } = 0f;
            public static float FrameUpdateElapsedTime { get; set; } = 0f;
            public static float FrameUpdateLastElapsedTime { get; set; } = 0f;
            public static TileStream[] DecodePendingTileStreams { get; set; } = Enumerable.Empty<TileStream>().ToArray();
            public static int UpdatePendingCount { get; set; } = 0;

            private static readonly Tracer Trace = new Tracer();

            public static void FrameUpdate()
            {
                var isJitter = false;
                var frameDeltaTime = FrameUpdateElapsedTime - FrameUpdateLastElapsedTime;
                if (frameDeltaTime > Config.JitterSpan)
                {
                    isJitter = true;
                    JitterCount++;
                }

                Trace.InfoMessage($"frame${FrameUpdateExecuteIndex}$$update$$jitter${isJitter}$$delta${frameDeltaTime}");
                if (FrameUpdateExecuteIndex++ == 0)
                {
                    FrameUpdateExpectTime = FrameUpdateElapsedTime;
                }

                FrameUpdateLastElapsedTime = FrameUpdateElapsedTime;
                FrameUpdateExpectTime += Config.UpdateSpan;
            }
        }

        [UsedImplicitly]
        private class PlayState
        {
            public static bool Playing { get; set; } = true;
        }
    }
}
