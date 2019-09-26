using JetBrains.Annotations;

#if UNITY_EDITOR
#endif

namespace Assets.Gallery.Scripts
{
    public partial class MediaPlayer
    {
        [UsedImplicitly]
        public class Config
        {
            private static int Fps { get; } = 30;
            public static float UpdateSpan { get; } = 1.0f / Fps;
            public static float JitterSpan { get; } = UpdateSpan * 3;
            public static int TexturePoolSize { get; set; } = 1;
            public static bool EnableMouseControl { get; } = false;
            public static bool EnableVr { get; } = !EnableMouseControl;
            public static bool EnableTraceFov { get; set; } = false;
        }
    }
}