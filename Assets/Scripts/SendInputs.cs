using System;
using System.Text;
using UnityEngine;
using System.Net.Sockets;

public class SendInputs : MonoBehaviour
{
    #region Singleton
    private static SendInputs _instance;

    public static SendInputs Instance
    {
        get { return _instance; }
    }

    private void Awake()
    {
        _instance = this;
    }
    #endregion

    [SerializeField] int _port; 
    [SerializeField] string _ip;
    
    void Start()
    {
        //byte[] aux = Encoding.ASCII.GetBytes(PlayerPrefs.GetString("IP"));
        //_ip = Encoding.ASCII.GetString(aux);
        //_ip = _ip.Substring(0, _ip.Length);
        _ip = PlayerPrefs.GetString("IP");
        _port = PlayerPrefs.GetInt("Port");
    }

    public void SendPacket(string message)
    {
        UdpClient udpClient = new UdpClient(_port);

        try
        {
            udpClient.Connect(_ip, _port);

            Byte[] sendBytes = Encoding.ASCII.GetBytes(message);

            // Sends a message to the host to which you have connected.
            udpClient.Send(sendBytes, sendBytes.Length);

            #region reciveData
            ////IPEndPoint object will allow us to read datagrams sent from any source.
            //IPEndPoint RemoteIpEndPoint = new IPEndPoint(IPAddress.Any, 0);

            //// Blocks until a message returns on this socket from a remote host.
            //var receiveBytes = udpClient.Receive(ref RemoteIpEndPoint);
            //string returnData = Encoding.ASCII.GetString(receiveBytes);
            //Debug.Log(returnData.ToString());
            #endregion

            udpClient.Close();
        }
        catch (Exception e)
        {
            Debug.Log(e);
        }
    }
}
