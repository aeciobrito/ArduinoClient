using UnityEngine;
using UnityEngine.UI;
using UnityEngine.SceneManagement;
using System;

public class ApplicationController : MonoBehaviour
{
    [SerializeField] InputField _ipInputFiled, _portInputField, _cameraInputField;
    [SerializeField] Slider _qualitySlider;

    // Use this for initialization
    void Start()
    {
        _ipInputFiled.text = PlayerPrefs.GetString("IP", "192.168.1.100");
        _portInputField.text = PlayerPrefs.GetInt("Port", 2000).ToString();
        _cameraInputField.text = PlayerPrefs.GetString("IPCam", "192.168.1.100");
        _qualitySlider.value = PlayerPrefs.GetInt("CamQuality", 50);
    }

    public void LoadScene(string sceneName)
    {
        SaveInputFileds();

        SceneManager.LoadScene(sceneName);
    }

    public void SaveInputFileds()
    {
        if (_ipInputFiled.text == "" || _portInputField.text == "")
            return;

        PlayerPrefs.SetString("IP", _ipInputFiled.text);
        PlayerPrefs.SetInt("Port", Convert.ToInt32(_portInputField.text));
        PlayerPrefs.SetString("IPCam", _cameraInputField.text);
        PlayerPrefs.SetInt("CamQuality", (int)_qualitySlider.value);
    }

    public void Quit()
    {
        Debug.Log("Saiu");
        Application.Quit();
    }
}
