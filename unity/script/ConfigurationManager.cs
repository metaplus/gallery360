using System.Collections;
using System.Collections.Generic;
using System.Linq;
using UnityEngine;

namespace Gallery
{
    public class ConfigurationManager : MonoBehaviour
    {
        public static int Test { get; set; }

        public static string FilePath { get; set; }

        private static string DefaultFileFolder
        {
            get { return "C:/Media/"; }
        }

        public static string DefaultFilePath
        {
            get { return DefaultFileFolder + "MercedesBenz4096x2048.mp4"; }
        }

        public static string MenuSceneName
        {
            get { return "VideoMenu"; }
        }

        public static string RenderSceneName
        {
            get { return "VrRender"; }
        }

        private void Awake()
        {

            GeneralSetting();
        }

        public static void GeneralSetting()
        {
            //Screen.SetResolution(1920, 1080, false);
            Screen.SetResolution(2560, 1440, false);
            QualitySettings.vSyncCount = 0;
            Application.targetFrameRate = 120;
#if !CURVEDUI_TMP
#error Require CURVEDUI_TMP defined in platform specific symbols list
#endif
        }
    }
}