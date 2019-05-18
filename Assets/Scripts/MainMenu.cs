using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;

public class MainMenu : MonoBehaviour
{
    [SerializeField] GameObject _mainMenu, _clientMenu, _serverMenu;
    private GameObject _currentMenu;

    void Start()
    {
        _mainMenu.SetActive(true);
        _clientMenu.SetActive(false);
        _clientMenu.SetActive(false);

        _currentMenu = _mainMenu;
    }

    public void ActiveMenu(GameObject menuToActive)
    {
        _currentMenu.SetActive(false);
        menuToActive.SetActive(true);
        _currentMenu = menuToActive;
    }
}
