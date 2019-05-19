using UnityEngine;
using UnityEngine.UI;
using UnityEngine.SceneManagement;
using System;

public class ApplicationController : MonoBehaviour
{
    [SerializeField] InputField _carIpInputFiled, _carPortInputField, _cameraInputField, _cameraPortInputField, _cameraServerPort;
    [SerializeField] Slider _qualitySlider;

    // Use this for initialization
    void Start()
    {
        _carIpInputFiled.text = PlayerPrefs.GetString("IP", "192.168.1.100");
        _carPortInputField.text = PlayerPrefs.GetInt("Port", 1987).ToString();

        _cameraInputField.text = PlayerPrefs.GetString("IPCam", "192.168.1.100");
        _cameraPortInputField.text = PlayerPrefs.GetInt("PortCam", 8080).ToString();

        _cameraServerPort.text = PlayerPrefs.GetInt("CameraServerPort", 8080).ToString();

        _qualitySlider.value = PlayerPrefs.GetInt("CamQuality", 50);
    }

    public void LoadScene(string sceneName)
    {
        SaveInputFileds();
        SceneManager.LoadScene(sceneName);
    }

    public void SaveInputFileds()
    {
        if (_carIpInputFiled.text == "" || _carPortInputField.text == "")
            return;

        PlayerPrefs.SetString("IP", _carIpInputFiled.text);
        PlayerPrefs.SetInt("Port", Convert.ToInt32(_carPortInputField.text));

        PlayerPrefs.SetString("IPCam", _cameraInputField.text);
        PlayerPrefs.SetInt("PortCam", Convert.ToInt32(_cameraPortInputField.text));

        PlayerPrefs.SetInt("CameraServerPort", Convert.ToInt32(_cameraServerPort.text));

        PlayerPrefs.SetInt("CamQuality", (int)_qualitySlider.value);
    }

    public void Quit()
    {
        Debug.Log("Saiu");
        Application.Quit();
    }
}
