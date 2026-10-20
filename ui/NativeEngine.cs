using System.Runtime.InteropServices;
using System.Text;
using Microsoft.Win32.SafeHandles;

namespace Elka.VM.EQ;

[StructLayout(LayoutKind.Sequential, Pack = 8)]
internal struct SpectrumFrame
{
    public int State, Requested, SampleRate, Kind, Index, Channel;
    public uint Sequence;
    public int AudioFrames;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 384)] public float[] Db;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 160)] public byte[] MessageBytes;
    public readonly string Message => Encoding.UTF8.GetString(MessageBytes).TrimEnd('\0');
}

[StructLayout(LayoutKind.Sequential, Pack = 8)]
internal struct Frame
{
    public int Edition, Kind, Index, Channels, Ready, Enabled, Bank;
    public uint Version;
    public double SampleRate;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 240)] public float[] Bands;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 200)] public byte[] StatusBytes;
    public readonly double Get(int channel, int band, int field) => Bands[channel * 30 + band * 5 + field];
    public readonly string Status => Encoding.UTF8.GetString(StatusBytes).TrimEnd('\0');
    public readonly bool Same(Frame other) => Edition == other.Edition && Kind == other.Kind && Index == other.Index &&
        Channels == other.Channels && Ready == other.Ready && Enabled == other.Enabled && Bank == other.Bank && SampleRate == other.SampleRate &&
        Bands.AsSpan().SequenceEqual(other.Bands) && StatusBytes.AsSpan().SequenceEqual(other.StatusBytes);
}

internal sealed class EngineHandle : SafeHandleZeroOrMinusOneIsInvalid
{
    private EngineHandle() : base(true) { }
    protected override bool ReleaseHandle() { Native.Destroy(handle); return true; }
}

internal static class Native
{
    private const string Dll = "ElkaEqEngine.dll";
    internal const int On = 0, Type = 1, Frequency = 2, Gain = 3, Q = 4;
    internal static readonly int FrameBytes = Marshal.SizeOf<Frame>();
    internal static readonly int SpectrumBytes = Marshal.SizeOf<SpectrumFrame>();
    [DllImport(Dll, EntryPoint = "ElkaEq_SpectrumFrameSize", CallingConvention = CallingConvention.Cdecl)] internal static extern int SpectrumFrameSize();
    [DllImport(Dll, EntryPoint = "ElkaEq_SpectrumRequest", CallingConvention = CallingConvention.Cdecl)] internal static extern int SpectrumRequest(EngineHandle h, int enabled, int kind, int index, int channel);
    [DllImport(Dll, EntryPoint = "ElkaEq_SpectrumRead", CallingConvention = CallingConvention.Cdecl)] internal static extern int SpectrumRead(EngineHandle h, out SpectrumFrame frame, int bytes);
    [DllImport(Dll, EntryPoint = "ElkaEq_AbiVersion", CallingConvention = CallingConvention.Cdecl)] internal static extern int AbiVersion();
    [DllImport(Dll, EntryPoint = "ElkaEq_FrameSize", CallingConvention = CallingConvention.Cdecl)] internal static extern int FrameSize();
    [DllImport(Dll, EntryPoint = "ElkaEq_Create", CallingConvention = CallingConvention.Cdecl)] internal static extern EngineHandle Create(int demo);
    [DllImport(Dll, EntryPoint = "ElkaEq_Destroy", CallingConvention = CallingConvention.Cdecl)] internal static extern void Destroy(IntPtr handle);
    [DllImport(Dll, EntryPoint = "ElkaEq_Read", CallingConvention = CallingConvention.Cdecl)] internal static extern int Read(EngineHandle h, out Frame frame, int bytes);
    [DllImport(Dll, EntryPoint = "ElkaEq_Select", CallingConvention = CallingConvention.Cdecl)] internal static extern int Select(EngineHandle h, int kind, int index);
    [DllImport(Dll, EntryPoint = "ElkaEq_Edit", CallingConvention = CallingConvention.Cdecl)] internal static extern int Edit(EngineHandle h, int channel, int band, int field, double value);
    [DllImport(Dll, EntryPoint = "ElkaEq_Enabled", CallingConvention = CallingConvention.Cdecl)] internal static extern int Enabled(EngineHandle h, int enabled);
    [DllImport(Dll, EntryPoint = "ElkaEq_Bank", CallingConvention = CallingConvention.Cdecl)] internal static extern int Bank(EngineHandle h, int bank);
    [DllImport(Dll, EntryPoint = "ElkaEq_Dragging", CallingConvention = CallingConvention.Cdecl)] internal static extern void Dragging(EngineHandle h, int dragging);
    [DllImport(Dll, EntryPoint = "ElkaEq_Apply", CallingConvention = CallingConvention.Cdecl)] internal static extern int Apply(EngineHandle h, int channel, float[] values, int channels, int count, int band);
    [DllImport(Dll, EntryPoint = "ElkaEq_Defaults", CallingConvention = CallingConvention.Cdecl)] internal static extern int Defaults(out Frame frame, int bytes);
    [DllImport(Dll, EntryPoint = "ElkaEq_TargetCount", CallingConvention = CallingConvention.Cdecl)] internal static extern int TargetCount(int edition, int kind);
    [DllImport(Dll, EntryPoint = "ElkaEq_Limit", CallingConvention = CallingConvention.Cdecl)] internal static extern double Limit(int edition, int kind, int field, double value);
    [DllImport(Dll, EntryPoint = "ElkaEq_Curve", CallingConvention = CallingConvention.Cdecl)] internal static extern int Curve(ref Frame frame, int bytes, int channel, int solo, int count, [Out] double[] output);
    internal static double[] Response(Frame frame, int channel, int solo = -1)
    {
        var values = new double[1024];
        if (Curve(ref frame, FrameBytes, channel, solo, values.Length, values) == 0) throw new InvalidOperationException("Invalid native EQ curve request.");
        return values;
    }
    internal static Frame Snapshot(EngineHandle h)
    {
        if (Read(h, out var frame, FrameBytes) == 0) throw new InvalidOperationException("Native EQ snapshot failed.");
        return frame;
    }
    internal static EngineHandle Open(bool demo)
    {
        if (AbiVersion() != 1 || FrameBytes != FrameSize() || SpectrumBytes != SpectrumFrameSize()) throw new InvalidOperationException("Native EQ DLL and interface versions do not match.");
        var handle = Create(demo ? 1 : 0);
        if (handle.IsInvalid) { handle.Dispose(); throw new InvalidOperationException("Could not start the native EQ backend."); }
        return handle;
    }
    internal static string TargetName(int edition, int kind, int index)
    {
        if (kind == 0) return $"IN {index + 1}";
        int hardware = edition == 3 ? 5 : edition == 2 ? 3 : 1;
        return index < hardware ? $"A{index + 1}" : $"B{index - hardware + 1}";
    }
}
