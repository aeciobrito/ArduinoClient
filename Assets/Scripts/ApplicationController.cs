using UnityEngine;
using UnityEngine.UI;
using UnityEngine.SceneManagement;
using System;

public class ApplicationController : MonoBehaviour
{
    [SerializeField] InputField ipInputFiled, portInputField;

    // Use this for initialization
    void Start ()
    {
        ipInputFiled.text = PlayerPrefs.GetString("IP", "192.168.1.100");
        portInputField.text = PlayerPrefs.GetInt("Port", 2000).ToString();
    }	

    public void LoadScene(string sceneName)
    {
        SaveInputFileds();

        SceneManager.LoadScene(sceneName);
    }

    public void SaveInputFileds()
    {
        if (ipInputFiled.text == "" || portInputField.text == "")
            return;

        PlayerPrefs.SetString("IP", ipInputFiled.text);
        PlayerPrefs.SetInt("Port", Convert.ToInt32(portInputField.text));
    }

    public void Quit()
    {
        Debug.Log("Saiu");
        Application.Quit();
    }
}
