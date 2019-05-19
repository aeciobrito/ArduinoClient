using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public abstract class Singleton<T> : MonoBehaviour where T : MonoBehaviour
{
    private static T _instace;

    public static T Instance
    {
        get
        {
            if (_instace == null)
                _instace = FindObjectOfType<T>();

            return _instace;
        }
    }
}
