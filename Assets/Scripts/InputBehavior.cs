using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class InputBehavior : MonoBehaviour
{
    private bool anyKeyDown = false;

    void Update()
    {
        KeyInputs();
        JoystickInput();        
    }

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
        float x, y, inputValue = 0f;

        x = Input.GetAxisRaw("Horizontal");
        y = Input.GetAxisRaw("Vertical");

        if (x == 0 && y == 0)
            SendInputs.Instance.SendPacket("S");
        else if (x == 0 && y > inputValue)
            SendInputs.Instance.SendPacket("W");
        else if (x > inputValue && y > inputValue)
            SendInputs.Instance.SendPacket("E");
        else if (x < -inputValue && y > inputValue)
            SendInputs.Instance.SendPacket("Q");
        else if (x == 0 && y < -inputValue)
            SendInputs.Instance.SendPacket("X");
        else if (x > inputValue && y < -inputValue)
            SendInputs.Instance.SendPacket("C");
        else if (x < -inputValue && y < -inputValue)
            SendInputs.Instance.SendPacket("Z");
        else if (x > inputValue && y == 0)
            SendInputs.Instance.SendPacket("D");
        else if (x < inputValue && y == 0)
            SendInputs.Instance.SendPacket("A");        
    }
}
