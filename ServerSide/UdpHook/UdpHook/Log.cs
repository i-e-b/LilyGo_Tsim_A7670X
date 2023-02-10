using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.Runtime.CompilerServices;
using System.Text;

namespace UdpHook;

[SuppressMessage("ReSharper", "UnusedMember.Global")]
public enum LogLevel
{
    None = 0,
    Error = 1,
    Warning = 2,
    Info = 3,
    Debug = 4,
    
    /// <summary>
    /// Include very verbose messages
    /// </summary>
    Trace = 5,
    
    /// <summary>
    /// Include raw data for debugging crypto
    /// </summary>
    Crypto = 100,
    
    /// <summary>
    /// Very important log that is always output
    /// </summary>
    Critical = 254,
    
    /// <summary>
    /// Output all logs
    /// </summary>
    Everything = 255
}

public static class Log
{
    private static LogLevel _level = LogLevel.Warning;
    
    public static bool IncludeCrypto => _level >= LogLevel.Crypto;
    public static bool IsTracing => _level >= LogLevel.Trace;
    public static bool IncludeInfo => _level >= LogLevel.Info;
    public static bool NotTraceOrDebug => _level <= LogLevel.Info;

    private static readonly object _lock = new();
    
    public static void SetLevel(LogLevel level)
    {
        Console.WriteLine($"Log level set to {(int)level} ({level.ToString()})");
        _level = level;
    }

    /// <summary>
    /// Write cryptographically sensitive log.
    /// This is never sent to any remote log servers.
    /// </summary>
    public static void Crypto(string msg)
    {
        if (_level < LogLevel.Crypto) return;
        lock (_lock)
        {
            Console.WriteLine(msg);
        }
    }

    public static void Trace(string msg)
    {
        if (_level < LogLevel.Trace) return;

        lock (_lock)
        {
            BlankTimestamp();
            Console.WriteLine(msg);
        }
    }
    public static void Trace(string msg, Func<string> more)
    {
        if (_level < LogLevel.Trace) return;

        lock (_lock)
        {
            BlankTimestamp();
            var sb = new StringBuilder();
            sb.Append(msg);
            sb.AppendLine(more());
            
            Console.Write(sb.ToString());
        }
    }
    public static void Trace(Func<string> msg)
    {
        if (_level < LogLevel.Trace) return;

        lock (_lock)
        {
            BlankTimestamp();
            var message = msg();
            Console.Write(message);
        }
    }
    
    public static void TraceWithStack(string msg)
    {
        if (_level < LogLevel.Trace) return;

        lock (_lock)
        {
            BlankTimestamp();
            Console.WriteLine(msg);
            WriteStack();
        }
    }

    public static void Debug(string msg, Func<IEnumerable<string>>? subLines = null)
    {
        if (_level < LogLevel.Debug) return;

        lock (_lock)
        {
            Timestamp();

            var sb = new StringBuilder();
            sb.AppendLine(msg);
            if (subLines is not null)
            {
                var lines = subLines();
                foreach (var line in lines)
                {
                    sb.Append("    ");
                    sb.AppendLine(line);
                }
            }

            Console.WriteLine(sb.ToString());
        }
    }
    
    public static void Debug(IEnumerable<string> messages)
    {
        if (_level < LogLevel.Debug) return;

        lock (_lock)
        {
            Timestamp();
            
            var sb = new StringBuilder();
            foreach (var msg in messages)
            {
                sb.Append(msg);
                sb.Append(" ");
            }
            sb.AppendLine();

            Console.WriteLine(sb.ToString());
        }
    }

    /// <summary>
    /// Log a debug message with a stack trace
    /// </summary>
    public static void DebugWithStack(string msg)
    {
        if (_level < LogLevel.Debug) return;

        lock (_lock)
        {
            Timestamp();

            Console.WriteLine(msg);
            WriteStack();
        }
    }

    public static void Info(string msg)
    {
        if (_level < LogLevel.Info) return;
        
        lock (_lock)
        {
            Timestamp();
            Console.WriteLine(msg);
        }
    }
    
    public static void Warn(string msg)
    {
        if (_level < LogLevel.Warning) return;

        lock (_lock)
        {
            Timestamp();
            Console.WriteLine(msg);
        }
    }

    public static void WarnWithStack(string msg)
    {        if (_level < LogLevel.Warning) return;

        lock (_lock)
        {
            Timestamp();
            Console.WriteLine(msg);
            WriteStack();
        }
    }
    
    public static void Error(string msg,
        [CallerMemberName] string memberName = "<unknown>",
        [CallerFilePath] string sourceFilePath = "",
        [CallerLineNumber] int sourceLineNumber = 0)
    {
        if (_level < LogLevel.Error) return;

        lock (_lock)
        {
            Timestamp();
            Console.WriteLine(msg);

            BlankTimestamp();
            Console.WriteLine($"In {memberName}, {sourceFilePath}::{sourceLineNumber}");
        }
    }
    

    public static void Error(string message, Exception ex)
    {
        if (_level < LogLevel.Error) return;

        lock (_lock)
        {
            Timestamp();

            if (_level >= LogLevel.Debug)
            {
                Console.WriteLine(message + ": " + ex); // full trace with debug
            }
            else
            {
                Console.WriteLine(message + ": " + ex.GetType().Name + " " + ex.Message); // just the top message if not debug
            }
        }
    }

    /// <summary>
    /// Always writes, regardless of log level
    /// </summary>
    public static void Critical(string msg)
    {
        lock (_lock)
        {
            Timestamp();
            Console.WriteLine();
            Console.WriteLine("##################################################");
            Console.WriteLine(msg);
            Console.WriteLine("##################################################");
            Console.WriteLine();
            WriteStack();
            Console.WriteLine();
        }
    }


    private static void WriteStack()
    {
        var st = new StackTrace(0);

        var frames = st.GetFrames();
        foreach (var frame in frames)
        {
            BlankTimestamp();
            var method = frame.GetMethod();
            Console.WriteLine($"    {method?.DeclaringType?.Name??"?"}.{method?.Name ?? "unknown"}");
        }
    }
    
    private static void Timestamp()
    {
        Console.Write(DateTime.UtcNow.ToString("yyyy-MM-ddTHH:mm"));
        Console.Write(" (utc) ");
    }

    private static void BlankTimestamp()
    {
        Console.Write("                       "); // same spacing as timestamp
    }
}