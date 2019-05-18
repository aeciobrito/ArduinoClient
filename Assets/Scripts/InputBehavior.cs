using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class InputBehavior : MonoBehaviour
{
    private bool anyKeyDown = false;

#if UNITY_EDITOR || UNITY_STANDALONE
    void Update()
    {
        KeyInputs();
        JoystickInput();        
    }
#endif

    public void KeyInputs()
    {
        if (Input.GetKeyDown(KeyCode.UpArrow))
        {
            SendInputs.Instance.SendPacket("W");
        }
        else if (Input.GetKeyDown(KeyCode.DownArrow))
        {
            SendInputs.Instance.SendPacket("X");
        }
        else if (Input.GetKeyDown(KeyCode.LeftArrow))
        {
            SendInputs.Instance.SendPacket("A");
        }
        else if (Input.GetKeyDown(KeyCode.RightArrow))
        {
            SendInputs.Instance.SendPacket("D");
        }

        if (Input.anyKey)
        {
            anyKeyDown = true;
        }
        else if (!Input.anyKey && anyKeyDown)
        {
            anyKeyDown = false;
            SendInputs.Instance.SendPacket("S");
        }
    }

    public void TouchButtonInputs(string key)
    {
        SendInputs.Instance.SendPacket(key);
    }

    public void JoystickInput()
    {
        float x, y, inputValue = 0.1f;

        x = Input.GetAxisRaw("Horizontal");
        y = Input.GetAxisRaw("Vertical");

        if (x == 0 && y == 0)
            Debug.Log("Stop");
        else if (x == 0 && y > inputValue)
            Debug.Log("Foward");
        else if (x > inputValue && y > inputValue)
            Debug.Log("FowardRight");
        else if (x < -inputValue && y > inputValue)
            Debug.Log("FowardLeft");
        else if (x == 0 && y < -inputValue)
            Debug.Log("Backward");
        else if (x > inputValue && y < -inputValue)
            Debug.Log("BackwardRight");
        else if (x < -inputValue && y < -inputValue)
            Debug.Log("BackwardLeft");
        else if (x > inputValue && y == 0)
            Debug.Log("Right");
        else if (x < inputValue && y == 0)
            Debug.Log("Left");

        //Debug.Log(x);
        //Debug.Log(y);
    }
}
