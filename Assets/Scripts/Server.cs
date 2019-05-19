﻿using UnityEngine;
using System.Collections;
using System.IO;
using UnityEngine.UI;
using System;
using System.Text;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using System.Collections.Generic;

public class Server : MonoBehaviour
{
    public CameraFeed cameraFeed;
    public bool enableLog = false;

    private Texture2D currentTexture;
    private TcpListener listner;
    [SerializeField] int port = 0;
    private bool stop = false;
    private IPAddress myIP;
    [Range(1,100)]
    [SerializeField] int imageQuality;
    [SerializeField] Text ipText;

    private List<TcpClient> clients = new List<TcpClient>();

    //This must be the-same with SEND_COUNT on the client
    const int SEND_RECEIVE_COUNT = 15;

    private void OnEnable()
    {
        Application.runInBackground = true;
        ipText.text = GetLocalIPAddress();
        port = PlayerPrefs.GetInt("CameraServerPort", 8080);
        Debug.Log(port);
        imageQuality = PlayerPrefs.GetInt("CamQuality");
        Debug.Log(imageQuality);

        //Start WebCam coroutine
        StartCoroutine(InitAndWaitForCamImage());
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

    IEnumerator InitAndWaitForCamImage()
    {
        listner = new TcpListener(IPAddress.Any, port);

        listner.Start();

        while (!cameraFeed.GetImageAvailable())
        {
            yield return new WaitForSeconds(1f);
        }
        currentTexture = new Texture2D(cameraFeed.getWidth(), cameraFeed.getHeight());
        Debug.Log("got Cam Image");
        //Start sending coroutine
        StartCoroutine(SenderCOR());
    }

    WaitForEndOfFrame endOfFrame = new WaitForEndOfFrame();
    IEnumerator SenderCOR()
    {
        bool isConnected = false;
        TcpClient client = null;
        NetworkStream stream = null;

        // Wait for client to connect in another Thread 
        Loom.RunAsync(() =>
        {
            while (client == null)
            {
                Debug.Log("Listening...");
                // Wait for client connection
                client = listner.AcceptTcpClient();
                if (client != null)
                {
                    // We are connected
                    clients.Add(client);

                    isConnected = true;
                    stream = client.GetStream();
                }
            }
        });

        //Wait until client has connected
        while (!isConnected)
        {
            yield return null;
        }

        LOG("Connected!");

        bool readyToGetFrame = true;

        byte[] frameBytesLength = new byte[SEND_RECEIVE_COUNT];

        while (!stop)
        {
            //Wait for End of frame
            yield return endOfFrame;
            currentTexture.SetPixels(cameraFeed.GetImage().GetPixels());
            byte[] pngBytes = currentTexture.EncodeToJPG(imageQuality);
            //Fill total byte length to send. Result is stored in frameBytesLength
            ByteLengthToFrameByteArray(pngBytes.Length, frameBytesLength);

            //Set readyToGetFrame false
            readyToGetFrame = false;

            Loom.RunAsync(() =>
            {
                //Send total byte count first
                stream.Write(frameBytesLength, 0, frameBytesLength.Length);
                LOG("Sent Image byte Length: " + frameBytesLength.Length);

                //Send the image bytes
                stream.Write(pngBytes, 0, pngBytes.Length);
                LOG("Sending Image byte array data : " + pngBytes.Length);

                //Sent. Set readyToGetFrame true
                readyToGetFrame = true;
            });

            //Wait until we are ready to get new frame(Until we are done sending data)
            while (!readyToGetFrame)
            {
                LOG("Waiting To get new frame");
                yield return null;
            }
        }
    }


    void LOG(string messsage)
    {
        if (enableLog)
            Debug.Log(messsage);
    }

    // stop everything
    private void OnDisable()
    {
        stop = true;

        if (listner != null)
        {
            listner.Stop();
            listner.Server.Close();
            listner = null;
        }

        foreach (TcpClient c in clients)
        {
            c.Client.Close();
            c.Close();
        }
        clients.Clear();
    }

    public static string GetLocalIPAddress()
    {
        var host = Dns.GetHostEntry(Dns.GetHostName());
        foreach (var ip in host.AddressList)
        {
            if (ip.AddressFamily == AddressFamily.InterNetwork)
            {
                return ip.ToString();
            }
        }

        throw new Exception("No network adapters with an IPv4 address in the system!");
    }
}
