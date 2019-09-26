using System;
using JetBrains.Annotations;
using UnityEngine;
using UnityEditor;

namespace Assets.Gallery.Scripts
{
    public partial class MediaPlayer
    {
        [UsedImplicitly]
        public class Tracer
        {
            public void InfoEvent(string instance, string eventContent)
            {
                if (!EnableTrace.Value) return;
                Native.TraceEvent(instance, eventContent);
            }

            public void InfoEvent(string instance, Func<string> eventContent)
            {
                if (!EnableTrace.Value) return;
                InfoEvent(instance, eventContent.Invoke());
            }

            public void InfoMessage(string message)
            {
                if (!EnableTrace.Value) return;
                Native.TraceMessage(message);
            }

            public void InfoMessage(Func<string> message)
            {
                if (!EnableTrace.Value) return;
                Native.TraceMessage(message.Invoke());
            }

            private static readonly Lazy<bool> EnableTrace = new Lazy<bool>(
                () => Native.TraceEvent("config", "verify.trace"), false);
        }
    }
}
