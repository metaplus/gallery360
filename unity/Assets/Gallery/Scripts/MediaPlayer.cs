using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Boo.Lang.Runtime;
using UnityEditor;
using UnityEngine;
using UnityEngine.Assertions;
using UnityEngine.Rendering;

namespace Assets.Gallery.Scripts
{
    public partial class MediaPlayer : MonoBehaviour
    {
        public GameObject LeftController;
        public GameObject RightController;

        public static int frameRow, frameCol;
        public static int frameWidth, frameHeight;

        [SerializeField] private GameObject sphereObject;
        [SerializeField] private Material sphereMaterial;
        [SerializeField] private int tileWidth, tileHeight;
        [SerializeField] private int tileUvWidth, tileUvHeight;

        private IList<TileStream> CreateTileList()
        {
            Assert.AreNotEqual(frameRow, 0);
            Assert.AreNotEqual(frameCol, 0);
            return Enumerable.Range(0, frameRow)
                .SelectMany(row =>
                {
                    return Enumerable.Range(0, frameCol)
                        .Select(col =>
                        {
                            var index = row * frameCol + col + 1;
                            var tileContext = new TileStream
                            {
                                Texture = new TextureBundle(tileWidth, tileHeight, false),
                                Col = col,
                                Row = row,
                                OffsetX = col * tileWidth,
                                OffsetY = row * tileHeight,
                                UvOffsetX = col * tileWidth / 2,
                                UvOffsetY = row * tileHeight / 2,
                                Index = index,
                                TextureIndex = (index - 1) * 3
                            };
                            Assert.AreEqual(tileContext.OffsetX % 2, 0);
                            Assert.AreEqual(tileContext.OffsetY % 2, 0);
                            Assert.IsNotNull(tileContext.Texture);
                            tileContext.Instance = Native.Dash.CreateTileStream(col, row, index,
                                IntPtr.Zero, IntPtr.Zero, IntPtr.Zero);
                            return tileContext;
                        });
                })
                .OrderBy(textureBundle => textureBundle.Index)
                .ToList();
        }

        private IList<TileStream> tileStreamList;
        private Queue<TextureBundle> frameTextureReadyQueue;
        private Queue<TextureBundle> frameTexturePrepareQueue;
        private TextureBundle frameTexture;
        private CommandBuffer commandBuffer;
        private Task pollTileTask;
        private CancellationTokenSource pollCancelTokenSource;
        private BlockingCollection<TileStream[]> readyTileCollection;
        private Tracer trace;

        private void Awake()
        {
            sphereObject = transform.parent.gameObject;
            var mesh = sphereObject.GetComponent<MeshFilter>().mesh;
            mesh.triangles = mesh.triangles.Reverse().ToArray();
            sphereMaterial = sphereObject.GetComponent<MeshRenderer>().material;
            Debug.Log("UpdateSpan " + Config.UpdateSpan + " JitterSpan " + Config.JitterSpan);
            Assert.raiseExceptions = true;
            Assert.IsTrue(SystemInfo.graphicsMultiThreaded, "graphicsMultiThreaded");
            Assert.IsTrue(SystemInfo.supportsGraphicsFence, "supportsGraphicsFence");
        }

        private GameObject FindSphere()
        {
            return GameObject.Find("RenderSphere");
        }

        private void Start()
        {
            var hasConfig = Native.LoadEnvConfig();
            Config.TexturePoolSize = int.Parse(Native.LoadEnvConfigEntry("System.TexturePoolSize"));
            Debug.Log("Config.TexturePoolSize " + Config.TexturePoolSize);
            Assert.IsTrue(hasConfig);
            Native.Initialize();
            var mpdUrl = Native.Dash.Create();
            Debug.Log("Mpd URL " + mpdUrl);
            Native.Dash.GraphicInfo(ref frameCol, ref frameRow, ref frameWidth, ref frameHeight);
            Debug.Log("Frame Width " + frameWidth + " Height " + frameHeight + " Col " + frameCol + " Row " + frameRow);
            Assert.IsTrue(frameWidth > 0 && frameWidth % frameCol == 0);
            Assert.IsTrue(frameHeight > 0 && frameHeight % frameRow == 0);
            frameTexture = new TextureBundle(frameWidth, frameHeight, true);
            frameTextureReadyQueue = new Queue<TextureBundle>(Config.TexturePoolSize);
            frameTexturePrepareQueue = Enumerable.Range(0, Config.TexturePoolSize)
                .Select(
                    index =>
                    {
                        var textureBundle = new TextureBundle(frameWidth, frameHeight, true)
                        {
                            Index = index
                        };
                        return textureBundle;
                    })
                .Aggregate(new Queue<TextureBundle>(Config.TexturePoolSize),
                    (prepareQueue, texture) =>
                    {
                        prepareQueue.Enqueue(texture);
                        return prepareQueue;
                    });
            Assert.AreEqual(frameTextureReadyQueue.Count, 0);
            Assert.AreEqual(frameTexturePrepareQueue.Count, Config.TexturePoolSize);
            sphereMaterial.SetTexture("_YTex", frameTexture.Y);
            sphereMaterial.SetTexture("_UTex", frameTexture.U);
            sphereMaterial.SetTexture("_VTex", frameTexture.V);
            tileWidth = frameWidth / frameCol;
            tileHeight = frameHeight / frameRow;
            tileUvWidth = tileWidth / 2;
            tileUvHeight = tileHeight / 2;
            Debug.Log("Tile Width " + tileWidth + " Height " + tileHeight);
            tileStreamList = CreateTileList();
            commandBuffer = new CommandBuffer();
            Native.Dash.Prefetch();
            Debug.Log("FrameRender launch");
            RenderState.UpdatePendingCount = tileStreamList.Count;
            trace = new Tracer();
            trace.InfoEvent("unity", "start");
            readyTileCollection = new BlockingCollection<TileStream[]>(tileStreamList.Count);
            pollCancelTokenSource = new CancellationTokenSource();
            pollTileTask = Task.Factory.StartNew(
                ProduceReadyTileStreams(pollCancelTokenSource.Token), TaskCreationOptions.LongRunning);
        }

        private Action ProduceReadyTileStreams(CancellationToken cancelToken)
        {
            return () =>
            {
                try
                {
                    while (true)
                    {
                        RenderState.DecodePendingTileStreams = tileStreamList.ToArray();
                        while (RenderState.DecodePendingTileStreams.Any() && !cancelToken.IsCancellationRequested)
                        {
                            var endOfStream = false;
                            var readyTileStreams = RenderState.DecodePendingTileStreams
                                .TakeWhile(
                                    tileStream =>
                                    {
                                        var updateResult = Native.Dash.PollTilePtrUpdate(
                                            tileStream.Instance, RenderState.FrameDecodeIndex,
                                            RenderState.DecodeBatchIndex);
                                        endOfStream |= (updateResult < 0);
                                        return updateResult > 0;
                                    })
                                .ToArray();
                            if (endOfStream)
                            {
                                Assert.AreEqual(0, readyTileStreams.Length);
                                readyTileCollection.CompleteAdding();
                                throw new RuntimeException("End of Stream");
                            }

                            if (readyTileStreams.Length <= 0) continue;
                            RenderState.DecodeBatchIndex++;
                            readyTileCollection.Add(readyTileStreams, cancelToken);
                            RenderState.DecodePendingTileStreams =
                                RenderState.DecodePendingTileStreams.Except(readyTileStreams).ToArray();
                        }

                        RenderState.FrameDecodeIndex++;
                    }
                }
                catch (Exception e)
                {
                    Debug.Log("Poll Task Exception " + e);
                }
                finally
                {
                    Debug.Log("Poll Task Decode Index " + RenderState.FrameDecodeIndex);
                }
            };
        }

        private static void LogIfElapsed(float span)
        {
            var currentTime = Time.time;
            if (RenderState.FrameUpdateExecuteIndex == 0)
            {
                RenderState.FrameLastTime = currentTime;
                return;
            }

            var logDeltaTime = currentTime - RenderState.FrameLastTime;
            if (logDeltaTime < span) return;
            RenderState.FrameLastTime = currentTime;
            Debug.Log("Update Frame Index " + RenderState.FrameUpdateExecuteIndex +
                      " Fps " + (RenderState.FrameUpdateExecuteIndex - RenderState.FrameLastIndex) / logDeltaTime +
                      " Jitter " + RenderState.JitterCount);
            RenderState.FrameLastIndex = RenderState.FrameUpdateExecuteIndex;
        }

        private void OverwriteMainTextures(TextureBundle sourceTexture)
        {
            commandBuffer.CopyTexture(sourceTexture.Y, frameTexture.Y);
            commandBuffer.CopyTexture(sourceTexture.U, frameTexture.U);
            commandBuffer.CopyTexture(sourceTexture.V, frameTexture.V);
        }

        private Action<TileStream> CacheTileTextures(TextureBundle targetTexture)
        {
            return tileStream =>
            {
                commandBuffer.IssuePluginCustomTextureUpdateV2(
                    Native.Graphic.GetTextureUpdateCallback(),
                    tileStream.Texture.Y, (uint) tileStream.TextureIndex);
                commandBuffer.IssuePluginCustomTextureUpdateV2(
                    Native.Graphic.GetTextureUpdateCallback(),
                    tileStream.Texture.U, (uint) tileStream.TextureIndex + 1);
                commandBuffer.IssuePluginCustomTextureUpdateV2(
                    Native.Graphic.GetTextureUpdateCallback(),
                    tileStream.Texture.V, (uint) tileStream.TextureIndex + 2);
                commandBuffer.CopyTexture(
                    tileStream.Texture.Y, 0, 0, 0, 0,
                    tileWidth, tileHeight, targetTexture.Y, 0, 0,
                    tileStream.OffsetX, tileStream.OffsetY);
                commandBuffer.CopyTexture(
                    tileStream.Texture.U, 0, 0, 0, 0,
                    tileUvWidth, tileUvHeight, targetTexture.U, 0, 0,
                    tileStream.UvOffsetX, tileStream.UvOffsetY);
                commandBuffer.CopyTexture(
                    tileStream.Texture.V, 0, 0, 0, 0,
                    tileUvWidth, tileUvHeight, targetTexture.V, 0, 0,
                    tileStream.UvOffsetX, tileStream.UvOffsetY);
            };
        }

        private void KeyBoardEventProcess()
        {
            if (Input.GetKeyDown(KeyCode.Escape))
            {
                Debug.Log("Key Down: Escape");
                enabled = false;
                return;
            }

            if (Input.GetKeyDown(KeyCode.Space))
            {
                Debug.Log("Key Down: Space");
                PlayState.Playing = !PlayState.Playing;
            }
        }

        private void Update()
        {
            KeyBoardEventProcess();
            RenderState.FrameUpdateElapsedTime += Time.deltaTime;
            if (!PlayState.Playing) return;
            if (frameTexturePrepareQueue.Count > 0)
            {
                var prepareTexture = frameTexturePrepareQueue.Peek();
                var tryCount = 0;
                while (!RenderState.FrameUpdateComplete && tryCount++ < 3 &&
                       readyTileCollection.TryTake(out var readyTileStreams))
                {
                    RenderState.UpdatePendingCount -= readyTileStreams.Length;
                    Array.ForEach(readyTileStreams, CacheTileTextures(prepareTexture));
                    RenderState.FrameUpdateComplete = RenderState.UpdatePendingCount == 0;
                }

                if (RenderState.FrameUpdateComplete)
                {
                    RenderState.FrameUpdateComplete = false;
                    RenderState.UpdatePendingCount = tileStreamList.Count;
                    trace.InfoMessage($"frame${ RenderState.FrameUpdateReadyIndex}$$ready");
                    RenderState.FrameUpdateReadyIndex++;
                    frameTextureReadyQueue.Enqueue(frameTexturePrepareQueue.Dequeue());
                }
            }

            if (frameTextureReadyQueue.Count > 0)
            {
                if (RenderState.FrameUpdateElapsedTime > RenderState.FrameUpdateExpectTime)
                {
                    var readyTexture = frameTextureReadyQueue.Dequeue();
                    OverwriteMainTextures(readyTexture);
                    frameTexturePrepareQueue.Enqueue(readyTexture);
                    RenderState.FrameUpdate();
                }
            }
            else if (readyTileCollection.IsCompleted)
            {
                Debug.Log("Video Drained");
                enabled = false;
                return;
            }

            Graphics.ExecuteCommandBuffer(commandBuffer);
            commandBuffer.Clear();
#if UNITY_EDITOR
            LogIfElapsed(3f);
#endif
        }

        private void OnDisable()
        {
            pollCancelTokenSource.Cancel();
            readyTileCollection.CompleteAdding();
            //pollTileTask.Wait();
            Debug.Log("Total Frame Update Index " + RenderState.FrameUpdateExecuteIndex);
            Native.Release();
#if UNITY_EDITOR
            EditorApplication.isPlaying = false;
#else
            Application.Quit();
#endif
            Debug.Log("Plugin Release");
        }
    }
}