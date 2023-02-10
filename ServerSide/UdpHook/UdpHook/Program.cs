using System.Net;
using System.Text;

namespace UdpHook;

internal static class Program
{
    public static void Main(string[]? args)
    {
        Log.SetLevel(LogLevel.Info);
        Log.Info("Starting UDP server");
        Log.Info("Type 'quit' and [ENTER] to close");
        using var server = new UdpServer();
        
        server.AddResponder(420, TestHandler);
        
        server.Start();

        while (true)
        {
            var msg = Console.ReadLine();
            if (msg?.ToLowerInvariant().Contains("quit") == true) break;
        }
        
        Log.Info("Stopping UDP server");
        server.Dispose();
    }

    private static void TestHandler(byte[] data, IPEndPoint remoteCaller, IUdpSender returnPath)
    {
        Log.Info($"Got message to port 420, from {remoteCaller.Address}:{remoteCaller.Port}");
        Log.Info(Encoding.UTF8.GetString(data));
        
        returnPath.SendData(Encoding.UTF8.GetBytes($"Hello from server, {remoteCaller.Address}:{remoteCaller.Port}!\n"));
    }
}