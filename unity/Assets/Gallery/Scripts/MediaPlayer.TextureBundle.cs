using System.Linq;
using UnityEngine;

namespace Assets.Gallery.Scripts
{
    public partial class MediaPlayer
    {
        private class TextureBundle
        {
            public Texture2D Y { get; }
            public Texture2D U { get; }
            public Texture2D V { get; }
            public int Index { get; set; } = -1;

            public TextureBundle(int width, int height, bool defaultColor)
            {
                Y = new Texture2D(width, height, TextureFormat.Alpha8, false, false);
                U = new Texture2D(width / 2, height / 2, TextureFormat.Alpha8, false, false);
                V = new Texture2D(width / 2, height / 2, TextureFormat.Alpha8, false, false);
                if (!defaultColor) return;
                MakeDefaultColor(Y, 0);
                MakeDefaultColor(U, 127);       //128
                MakeDefaultColor(V, 127);       //128
            }

            private static Texture2D CreateNativeTexture(int width, int height, sbyte value)
            {
                var texturePtr = Native.Graphic.CreateAlphaTexture(width, height, value);
                var texture = Texture2D.CreateExternalTexture(
                    width, height, TextureFormat.Alpha8, false, false, texturePtr);
                return texture;
            }

            private static void MakeDefaultColor(Texture2D texture, byte color)
            {
                texture.SetPixels32(
                    Enumerable.Repeat(
                        new Color32(0, 0, 0, color),
                        texture.GetPixels().Length).ToArray());
                texture.Apply();
            }
        }
    }
}