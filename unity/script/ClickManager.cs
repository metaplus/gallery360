using System.Collections;
using System.Collections.Generic;
using Gallery;
using UnityEngine;
using UnityEngine.SceneManagement;

namespace Gallery
{
    public class ClickManager : MonoBehaviour
    {
        public void SetFilePath(string filePath)
        {
            ConfigurationManager.FilePath = filePath;
        }

        public void SwitchRenderScene()
        {
            SceneManager.LoadScene(ConfigurationManager.RenderSceneName, LoadSceneMode.Single);
        }
    }
}

