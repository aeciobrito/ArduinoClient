using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class InputBehavior : MonoBehaviour
{

    private bool anyKeyDown = false;

    #if UNITY_EDITOR || UNITY_STANDALONE
    void Update()
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
    #endif
    public void TouchButtonInputs(string key)
    {
        SendInputs.Instance.SendPacket(key);
    }
}
