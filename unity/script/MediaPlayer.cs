using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
#if UNITY_EDITOR
using UnityEditor;
#endif
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.SceneManagement;
using Valve.VR;

namespace Gallery
{
    public partial class MediaPlayer : MonoBehaviour
    {
        [Serializable]
        private class MediaSession : object, IDisposable
        {
            [SerializeField] private int _width;
            [SerializeField] private int _height;
            private Texture2D _yTexture;
            private Texture2D _uTexture;
            private Texture2D _vTexture;
            [SerializeField] private string _url;
            [SerializeField] private ulong _hashId;

            private enum Status
            {
                Active,
                Exhausted,
                Deactive
            }

            [SerializeField] private Status _status;
            private bool _disposed;

            public MediaSession(string url)
            {
                _url = url;
                _disposed = false;
                _hashId = Native.Media.CreateSession(url);
                _status = Status.Active;
                Native.Media.GetSessionResolution(_hashId, ref _width, ref _height);
                print("Video Url " + _url + " Resolution " + _width + "x" + _height);
            }

            #region Controller Methods

            public bool Flush
            {
                set
                {
                    if (!value || _status == Status.Deactive) return;
                    _status = Status.Deactive;
                    Native.Media.ReleaseSession(_hashId);
                    _hashId = 0;
                    _width = 0;
                    _height = 0;
                    _url = "";
                    _yTexture = null;
                    _uTexture = null;
                    _vTexture = null;
                }
                get { return _status == Status.Deactive; }
            }

            #endregion

            #region Destructor Methods

            ~MediaSession()
            {
                Dispose(false);
            }

            public void Dispose()
            {
                if (_disposed) return;
                Dispose(true);
                GC.SuppressFinalize(this);
            }

            private void Dispose(bool disposing)
            {
                _status = Status.Deactive;
                _disposed = true;
                Native.Media.ReleaseSession(_hashId);
            }

            #endregion

            public static void InvokeRenderEvent(int eventId = 0)
            {
                lock (typeof(Native.Graphic))
                {
                    GL.IssuePluginEvent(Native.Graphic.GetRenderEventFunc(), eventId);
                }
            }

            public bool Available
            {
                get
                {
                    var isAvailable = Native.Media.IsSessionHasNextFrame(_hashId);
                    if (!isAvailable) _status = Status.Exhausted;
                    return isAvailable;
                }
            }

            public Material Material
            {
                set
                {
                    _yTexture = new Texture2D(_width, _height, TextureFormat.Alpha8, false);
                    _uTexture = new Texture2D(_width / 2, _height / 2, TextureFormat.Alpha8, false);
                    _vTexture = new Texture2D(_width / 2, _height / 2, TextureFormat.Alpha8, false);
                    lock (typeof(Native.Graphic))
                    {
                        value.SetTexture("_YTex", _yTexture);
                        value.SetTexture("_UTex", _uTexture);
                        value.SetTexture("_VTex", _vTexture);
                        Native.Graphic.SetAlphaTexture(_yTexture.GetNativeTexturePtr(), _uTexture.GetNativeTexturePtr(),
                            _vTexture.GetNativeTexturePtr());
                    }
                }
            }
        }

        private CVRCompositor _compositor;
        private Quaternion _rotation;
        private Dictionary<Spatial.Index, MediaSession> _mediaCollection;
        private uint _lastVrFrameIndex = uint.MaxValue;
        private uint _count;
        private bool _toAnotherScene;

        public bool UseVrDevice = false;
        public GameObject SphereObject;
        [SerializeField] private Material _sphereMaterial;
        [SerializeField] private MediaSession _mediaSession;
        [SerializeField] private bool _hasPaused;

        private IEnumerator FrameRender()
        {
            while (_mediaSession.Available)
            {
                if (!_hasPaused)
                {
                    transform.rotation = _rotation;
                    MediaSession.InvokeRenderEvent();
                }
                yield return new WaitForEndOfFrame();
            }
            print("Video Drained");
            enabled = false;
        }

        private void Awake()
        {
            _rotation = transform.rotation;
            if (SphereObject == null)
                SphereObject = GameObject.Find("RenderSphere");
            var mesh = SphereObject.GetComponent<MeshFilter>().mesh;
            mesh.triangles = mesh.triangles.Reverse().ToArray();
            _sphereMaterial = SphereObject.GetComponent<MeshRenderer>().material;
            ConfigurationManager.GeneralSetting();
            // Valve.VR.OpenVR.Compositor.FadeToColor(0.5f, 0, 0, 0, 1, false);
            Debug.Assert(SystemInfo.graphicsDeviceType == GraphicsDeviceType.Direct3D11);
            _hasPaused = false;
        }

        private IEnumerator Start()
        {
            Native.Media.Create();
            var filePath = ConfigurationManager.FilePath == null || ConfigurationManager.FilePath.Equals("") ?
                ConfigurationManager.DefaultFilePath : ConfigurationManager.FilePath;
            _mediaCollection = new Dictionary<Spatial.Index, MediaSession>();
            _mediaSession = new MediaSession(filePath) { Material = _sphereMaterial };
            _mediaCollection.Add(new Spatial.Index(0, 0), _mediaSession);
            yield return StartCoroutine(FrameRender());
        }

        private void Update()
        {
            if (Input.anyKeyDown)
            {
                print("Got Key Down, Count: " + ++_count + ", Player " + (!_hasPaused ? "Paused" : "Playing"));
                if (Input.GetKeyDown(KeyCode.Escape))
                {
                    print("Got Key Down, Code: Escape");
                    _hasPaused = true;
                    _toAnotherScene = true;
                    enabled = false;
                    ConfigurationManager.Test = (int)_count;
                    SceneManager.LoadScene(ConfigurationManager.MenuSceneName, LoadSceneMode.Single);
                    return;
                }

                _hasPaused = !_hasPaused;
            }

            if (UseVrDevice && _compositor == null)
                _compositor = SteamVR.instance.compositor;
            var frameTiming = new Compositor_FrameTiming
            {
                m_nSize = (uint)Marshal.SizeOf(typeof(Compositor_FrameTiming))
            };
            if (!UseVrDevice || _compositor != null && _compositor.GetFrameTiming(ref frameTiming, 0))
            {
                if (frameTiming.m_nFrameIndex == _lastVrFrameIndex) return;
                _lastVrFrameIndex = frameTiming.m_nFrameIndex;
            }
        }

        private void OnDisable()
        {
            if (!UseVrDevice || _compositor != null)
            {
                var vrStats = new Compositor_CumulativeStats();
                if (_compositor != null)
                    _compositor.GetCumulativeStats(ref vrStats, (uint)Marshal.SizeOf(typeof(Compositor_CumulativeStats)));
            }
            StopAllCoroutines();
            _mediaSession = null;
            _mediaCollection = null;
            Native.Media.Release();
            Native.Graphic.Release();
            print("Object Disabled");
            if (_toAnotherScene) return;
#if UNITY_EDITOR
            EditorApplication.isPlaying = false;
#else
        Application.Quit();
#endif
        }

        private void OnApplicationQuit()
        {
            print("Application Quit");
        }
    }

}