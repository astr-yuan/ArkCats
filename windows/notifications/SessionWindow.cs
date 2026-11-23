using System;
using System.Runtime.InteropServices;
using System.Windows.Forms;

// This hidden top-level window receives power/session broadcasts on the STA message pump.
// The helper never creates a taskbar or tray entry.
internal sealed class SessionWindow : NativeWindow, IDisposable
{
    public event Action Changed;
    private bool registered;
    private bool suspended;

    [DllImport("wtsapi32.dll")]
    private static extern bool WTSRegisterSessionNotification(IntPtr window, uint flags);
    [DllImport("wtsapi32.dll")]
    private static extern bool WTSUnRegisterSessionNotification(IntPtr window);
    [DllImport("wtsapi32.dll", CharSet = CharSet.Unicode)]
    private static extern bool WTSQuerySessionInformation(IntPtr server, int sessionId, int infoClass, out IntPtr data, out int bytes);
    [DllImport("wtsapi32.dll")]
    private static extern void WTSFreeMemory(IntPtr data);

    // Prefix of WTSINFOEXW for x64 Windows 10/11. Its Level1 union starts on an 8-byte boundary.
    [StructLayout(LayoutKind.Explicit)]
    private struct SessionInfo
    {
        [FieldOffset(0)] public uint Level;
        [FieldOffset(12)] public int ConnectionState;
        [FieldOffset(16)] public int SessionFlags;
    }

    public SessionWindow()
    {
        CreateHandle(new CreateParams { Caption = "ChristineNotifications", Style = unchecked((int)0x80000000) });
        registered = WTSRegisterSessionNotification(Handle, 0); // NOTIFY_FOR_THIS_SESSION
    }

    public bool Active()
    {
        if (suspended) return false;
        if (!registered) registered = WTSRegisterSessionNotification(Handle, 0);
        IntPtr data;
        int bytes;
        if (!WTSQuerySessionInformation(IntPtr.Zero, -1, 25, out data, out bytes)) return false;
        try
        {
            if (bytes < Marshal.SizeOf(typeof(SessionInfo))) return false;
            var info = (SessionInfo)Marshal.PtrToStructure(data, typeof(SessionInfo));
            return info.Level == 1 && info.ConnectionState == 0 && info.SessionFlags == 1;
        }
        finally { WTSFreeMemory(data); }
    }

    protected override void WndProc(ref Message m)
    {
        base.WndProc(ref m);
        bool changed = false;
        if (m.Msg == 0x02B1) changed = true; // WM_WTSSESSION_CHANGE
        if (m.Msg == 0x0218)
        {
            var kind = m.WParam.ToInt64();
            if (kind == 4 || kind == 6 || kind == 7 || kind == 18)
            {
                suspended = kind == 4;
                changed = true;
            }
        }
        if (changed && Changed != null) Changed();
    }

    public void Dispose()
    {
        if (registered) WTSUnRegisterSessionNotification(Handle);
        DestroyHandle();
    }
}
