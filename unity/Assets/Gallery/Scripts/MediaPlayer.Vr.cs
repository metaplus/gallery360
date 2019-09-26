using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using JetBrains.Annotations;

namespace Assets.Gallery.Scripts
{
    public partial class MediaPlayer
    {
        [UsedImplicitly]
        public class Vr
        {
            private static void ReserveMethod()
            {
                //if (UseVrDevice && _compositor == null)
                //{
                //    _compositor = SteamVR.instance.compositor;
                //}
                //var frameTiming = new Compositor_FrameTiming
                //{
                //    m_nSize = (uint)Marshal.SizeOf(typeof(Compositor_FrameTiming))
                //};
                //if (UseVrDevice && _compositor != null && _compositor.GetFrameTiming(ref frameTiming, 0))
                //{
                //    if (frameTiming.m_nFrameIndex == _lastVrFrameIndex) return;
                //    _lastVrFrameIndex = frameTiming.m_nFrameIndex;
                //}

                //if (!UseVrDevice || _compositor != null)
                //{
                //    var vrStats = new Compositor_CumulativeStats();
                //    if (_compositor != null)
                //    {
                //        _compositor.GetCumulativeStats(ref vrStats,
                //            (uint) Marshal.SizeOf(typeof(Compositor_CumulativeStats)));
                //    }
                //}
            }
        }
    }
}