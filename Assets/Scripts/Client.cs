using UnityEngine;
using System.Collections;
using UnityEngine.UI;
using System.Net.Sockets;
using System.Net;
using System.IO;
using System;

public class Client : MonoBehaviour
{
    public RawImage image;
    public bool enableLog = false;

    [SerializeField] int port = 0;
    [SerializeField] string phoneIP = "";//mobile phone IPaddress
    TcpClient client;

    Texture2D tex;

    private bool stop = false;

    //This must be the-same with SEND_COUNT on the server
    const int SEND_RECEIVE_COUNT = 15;

    // Use this for initialization
    IEnumerator Start()
    {
        Application.runInBackground = true;

        phoneIP = PlayerPrefs.GetString("IPCam");
        port = PlayerPrefs.GetInt("PortCam");


        tex = new Texture2D(0, 0);
        client = new TcpClient();

        while (!client.Connected)
        {
            try
            {
                LOGWARNING("Connecting to server...");
                client.Connect(IPAddress.Parse(phoneIP), port);
            }
            catch (SocketException s)
            {
                Debug.Log("Waiting for connection..." + s);

            }
            yield return new WaitForSeconds(1f);
        }
        LOGWARNING("Connected!");
        ImageReceiver();
    }


    void ImageReceiver()
    {
        //While loop in another Thread is fine so we don't block main Unity Thread
        Loom.RunAsync(() =>
       {
           while (!stop)
           {
                //Read Image Count
                int imageSize = ReadImageByteSize(SEND_RECEIVE_COUNT);
               LOGWARNING("Received Image byte Length: " + imageSize);

                //Read Image Bytes and Display it
                ReadFrameByteArray(imageSize);
           }
       });
    }


    //Converts the data size to byte array and put result to the fullBytes array
    void ByteLengthToFrameByteArray(int byteLength, byte[] fullBytes)
    {
        //Clear old data
        Array.Clear(fullBytes, 0, fullBytes.Length);
        //Convert int to bytes
        byte[] bytesToSendCount = BitConverter.GetBytes(byteLength);
        //Copy result to fullBytes
        bytesToSendCount.CopyTo(fullBytes, 0);
    }

    //Converts the byte array to the data size and returns the result
    int FrameByteArrayToByteLength(byte[] frameBytesLength)
    {
        int byteLength = BitConverter.ToInt32(frameBytesLength, 0);
        return byteLength;
    }


    /////////////////////////////////////////////////////Read Image SIZE from Server///////////////////////////////////////////////////
    private int ReadImageByteSize(int size)
    {
        bool disconnected = false;

        NetworkStream serverStream = client.GetStream();
        byte[] imageBytesCount = new byte[size];
        var total = 0;
        do
        {
            var read = serverStream.Read(imageBytesCount, total, size - total);
            if (read == 0)
            {
                disconnected = true;
                break;
            }
            total += read;
        } while (total != size);

        int byteLength;

        if (disconnected)
        {
            byteLength = -1;
        }
        else
        {
            byteLength = FrameByteArrayToByteLength(imageBytesCount);
        }
        imageBytesCount = null;
        return byteLength;
    }

    /////////////////////////////////////////////////////Read Image Data Byte Array from Server///////////////////////////////////////////////////
    private void ReadFrameByteArray(int size)
    {
        bool disconnected = false;

        NetworkStream serverStream = client.GetStream();
        byte[] imageBytes = new byte[size];
        var total = 0;
        do
        {
            var read = serverStream.Read(imageBytes, total, size - total);
            if (read == 0)
            {
                disconnected = true;
                break;
            }
            total += read;
        } while (total != size);

        bool readyToReadAgain = false;

        //Display Image
        if (!disconnected)
        {
            //Display Image on the main Thread
            Loom.QueueOnMainThread(() =>
           {
               DisplayReceivedImage(imageBytes);
               readyToReadAgain = true;
           });
        }

        //Wait until old Image is displayed
        while (!readyToReadAgain)
        {
            System.Threading.Thread.Sleep(1);
        }
    }

    void DisplayReceivedImage(byte[] receivedImageBytes)
    {
        tex.LoadImage(receivedImageBytes);
        image.texture = tex;
    }

    void LOG(string messsage)
    {
        if (enableLog)
            Debug.Log(messsage);
    }

    void LOGWARNING(string messsage)
    {
        if (enableLog)
            Debug.LogWarning(messsage);
    }

    void OnApplicationQuit()
    {
        LOGWARNING("OnApplicationQuit");
        stop = true;

        if (client != null)
        {
            client.Client.Close();
            client.Close();
        }
    }
}