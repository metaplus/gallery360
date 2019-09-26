using System;
using JetBrains.Annotations;
using UnityEngine;
using UnityEngine.Assertions;

namespace Assets.Gallery.Scripts
{
    public class CameraRotate : MonoBehaviour
    {
        private Camera mainCamera;
        private GameObject sphereObject;
        public static float angleX = 90;
        public static float angleY = 180;
        private MediaPlayer.Tracer tracer;

        private void Start()
        {
            mainCamera = Camera.main;
            Assert.IsNotNull(mainCamera);
            mainCamera.fieldOfView = MediaPlayer.Config.EnableMouseControl ? 90 : 110;

            sphereObject = GameObject.Find("RenderSphere");
            tracer = new MediaPlayer.Tracer();
            Debug.Log("Camera  " + mainCamera.name +
                      " Field-of-View " + mainCamera.fieldOfView);
        }

        Vector3 position()
        {
            return mainCamera.transform.position;
        }

        private void Update()
        {
            if (MediaPlayer.Config.EnableMouseControl)
            {
                mainCamera.transform.Rotate(
                    new Vector3(
                        Input.GetAxis("Mouse Y"),
                        Input.GetAxis("Mouse X"), 0
                    ) * Time.smoothDeltaTime * 300);
            }

            if (!MediaPlayer.Config.EnableTraceFov) return;
            sphereObject.transform.position = mainCamera.transform.position;
            var cameraAngles = mainCamera.transform.rotation.eulerAngles;
            angleX = cameraAngles.x;
            angleY = cameraAngles.y;

            var y = 360f - angleY;
            float x = 0;
            if (angleX <= 90)
            {
                x = angleX + 90f;
            }
            else if (angleX > 90 && angleX < 270)
            {
                x = 270f - angleX;
            }
            else
            {
                x = angleX - 270f;
            }

            var col = Math.Min((int) (y / 360f * MediaPlayer.frameCol), MediaPlayer.frameCol - 1);
            var row = Math.Min((int) (x / 180f * MediaPlayer.frameRow), MediaPlayer.frameRow - 1);
            Native.Dash.NotifyFieldOfView(col, row);
            tracer.InfoEvent("fov", () => $"y{angleY}x{angleX}col{col}row{row}");
        }
    };
}
