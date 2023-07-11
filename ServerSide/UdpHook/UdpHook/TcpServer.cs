using System.Net;
using System.Net.Sockets;

namespace UdpHook;

internal class TcpServer : IDisposable
{
    /// <summary>
    /// Pattern for a TCP server responder
    /// </summary>
    /// <param name="client">Connection client</param>
    /// <param name="remoteCaller">The address and port of remote side</param>
    public delegate void TcpResponder(TcpClient client, IPEndPoint remoteCaller);
    
    private volatile bool _running;
    
    private readonly Dictionary<int, Responder> _responders = new();
    
    private class Responder
    {
        public TcpResponder Action { get; }
        public TcpListener Client { get; }
        public Thread? WaitThread { get; set; }

        public Responder(int port, TcpResponder action)
        {
            Action = action;
            Client = new TcpListener(new IPEndPoint(IPAddress.Any, port));
        }
    }

    public void AddResponder(int port, TcpResponder action)
    {
        if (_responders.ContainsKey(port)) throw new Exception("This port is already bound");
        var responder = new Responder(port, action);
        _responders.Add(port, responder);

        responder.WaitThread = new Thread(() => { ListenLoop(port); }) { IsBackground = true, Name = $"TcpWaitThread_Port_{port}" };
    }

    private void ListenLoop(int port)
    {
        var responder = _responders[port];
        var sender = new IPEndPoint(IPAddress.Any, 0);
        
        responder.Client.Start();
        Log.Info($"Listening for messages on port {port}...");
        while (_running)
        {
            try
            {
                if (!responder.Client.Pending())
                {
                    Thread.Sleep(100);
                    continue; // check we are still running
                }

                using var client = responder.Client.AcceptTcpClient();
                responder.Action(client, sender);
                
                Log.Info("TCP transaction complete");
            }
            catch (Exception ex)
            {
                Log.Error($"Failure in ListenLoop loop for port {port}", ex);
            }
        }
        Log.Info($"Closing listener for port {port}...");
    }

    public void Start()
    {
        _running = true;

        foreach (var responder in _responders.Values)
        {
            responder.WaitThread?.Start();
        }
    }

    public void Dispose()
    {
        _running = false;
       
        foreach (var responder in _responders.Values)
        {
            responder.Client.Stop();
        }
        GC.SuppressFinalize(this);
    }
}