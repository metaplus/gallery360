using System;

#if UNITY_EDITOR
#endif

namespace Assets.Gallery.Scripts
{
    public partial class MediaPlayer
    {
        private class TileStream
        {
            public TextureBundle Texture { get; set; }
            public int Col { get; set; }
            public int Row { get; set; }
            public int OffsetX { get; set; }
            public int OffsetY { get; set; }
            public int UvOffsetX { get; set; }
            public int UvOffsetY { get; set; }
            public int Index { get; set; }
            public int TextureIndex { get; set; }
            public IntPtr Instance { get; set; }
        }
    }
}