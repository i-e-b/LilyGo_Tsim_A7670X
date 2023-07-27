using System.Net;
using System.Net.Sockets;
using System.Text;

namespace UdpHook;

internal static class Program
{
    private static IUdpSender? _lastReturn;
    private static volatile bool _holdOpen;

    public static void Main(string[]? args)
    {
        Log.SetLevel(LogLevel.Info);
        Log.Info("Starting UDP/TCP servers");
        Log.Info("Type 'quit' and [ENTER] to shutdown servers");
        Log.Info("Type 'close' and [ENTER] to close persistent TCP");
        using var udpServer = new UdpServer();
        using var tcpServer = new TcpServer();

        _holdOpen = false;

        udpServer.AddResponder(420, TestUdpHandler);
        tcpServer.AddResponder(421, TestTcpHandler);

        udpServer.Start();
        tcpServer.Start();

        while (true)
        {
            var msg = Console.ReadLine();
            if (msg?.ToLowerInvariant().Contains("quit") == true) break;
            if (msg?.ToLowerInvariant().Contains("close") == true) _holdOpen = false;
            if (msg?.ToLowerInvariant().Contains("ping") == true) _lastReturn?.SendData(Encoding.UTF8.GetBytes("Ping from server"));
        }

        Log.Info("Stopping UDP/TCP servers");
        udpServer.Dispose();
    }

    private static void TestTcpHandler(TcpClient client, IPEndPoint remoteCaller)
    {
        Log.Info($"Received TCP message from {remoteCaller.Address}:{remoteCaller.Port}");
        Log.Info($"Is connected = {client.Connected}");

        var stream = client.GetStream();

        _holdOpen = true;
        while (_holdOpen && client.Connected)
        {
            try
            {
                for (var i = 0; i < 10; i++)
                {
                    if (stream.DataAvailable) break;
                    Thread.Sleep(250);
                }

                Log.Info($"Is connected = {client.Connected}");
                Log.Info($"Can read = {stream.CanRead}. Data available = {stream.DataAvailable}");

                if (stream.DataAvailable)
                {
                    var buf = new List<byte>();
                    while (stream.DataAvailable)
                    {
                        try
                        {
                            var b = stream.ReadByte();
                            if (b >= 0) buf.Add((byte)b);
                            else break;
                        }
                        catch (Exception ex)
                        {
                            Log.Error("Failed while reading incoming data", ex);
                            break;
                        }
                    }

                    Log.Info($"Remote message = '{Encoding.UTF8.GetString(buf.ToArray())}'");
                }
                else
                {
                    Log.Info("No data from remote yet");
                }

                stream.Write(Encoding.UTF8.GetBytes($"Your message from {remoteCaller.Address}:{remoteCaller.Port} was received by the server."));
                stream.Flush();
            }
            catch (Exception exo)
            {
                Log.Error("Exception in outer loop", exo);
            }

            Thread.Sleep(500);
        }
    }

    private static void TestUdpHandler(byte[] data, IPEndPoint remoteCaller, IUdpSender returnPath)
    {
        var msgStr = Encoding.UTF8.GetString(data);
        Log.Info($"Got message to port 420, from {remoteCaller.Address}:{remoteCaller.Port}");
        Log.Info(msgStr);

        _lastReturn = returnPath;
        returnPath.SendData(Encoding.UTF8.GetBytes($"Reply from server. You are {remoteCaller.Address}:{remoteCaller.Port}; You said \"{msgStr}\"\n"));
    }
}