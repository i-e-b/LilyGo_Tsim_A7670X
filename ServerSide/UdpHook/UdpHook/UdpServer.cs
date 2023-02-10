using System.Net;
using System.Net.Sockets;

namespace UdpHook;

public class UpdSender : IUdpSender
{
    private readonly UdpClient _connection;
    private readonly IPEndPoint _target;
    private readonly UdpServer _parent;

    public UpdSender(UdpClient connection, IPEndPoint target, UdpServer parent)
    {
        _connection = connection;
        _target = target;
        _parent = parent;
    }
    
    public void SendData(byte[] data)
    {
        _parent.TotalOut += (ulong)data.Length;
        _connection.Send(data, data.Length, _target);
    }
}

public interface IUdpSender
{
    void SendData(byte[] data);
}

public class UdpServer : IDisposable
{
    /// <summary>
    /// Pattern for a UDP server responder
    /// </summary>
    /// <param name="data">Incoming data from remote caller</param>
    /// <param name="remoteCaller">The address and port of remote side</param>
    /// <param name="returnPath">Use this to send data back to the remote caller</param>
    public delegate void UdpResponder(byte[] data, IPEndPoint remoteCaller, IUdpSender returnPath);
    
    private volatile bool _running;
    
    private readonly Dictionary<int, Responder> _responders = new();
    
    public ulong TotalIn { get; set; }
    public ulong TotalOut { get; set; }

    public UdpServer()
    {
        TotalIn = 0;
        TotalOut = 0;
    }

    private class Responder
    {
        public UdpResponder Action { get; }
        public UdpClient Client { get; }
        public Thread? WaitThread { get; set; }

        public Responder(int port, UdpResponder action)
        {
            Action = action;
            Client = new UdpClient(new IPEndPoint(IPAddress.Any, port));
        }
    }

    public void AddResponder(int port, UdpResponder action)
    {
        if (_responders.ContainsKey(port)) throw new Exception("This port is already bound");
        var responder = new Responder(port, action);
        _responders.Add(port, responder);

        responder.WaitThread = new Thread(() => { ListenLoop(port); }) { IsBackground = true, Name = $"UdpWaitThread_Port_{port}" };
    }

    private void ListenLoop(int port)
    {
        var responder = _responders[port];
        var sender = new IPEndPoint(IPAddress.Any, 0);
        while (_running)
        {
            try
            {
                Log.Info($"Listening for messages on port {port}...");
                var buffer = responder.Client.Receive(ref sender);
                TotalIn += (ulong)buffer.Length;
                var returnPath = new UpdSender(responder.Client, sender, this);
                responder.Action(buffer, sender, returnPath);
            }
            catch (Exception ex)
            {
                Log.Error("Failure in IKE loop", ex);
            }
        }
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
            responder.Client.Dispose();
        }
        GC.SuppressFinalize(this);
    }
}
