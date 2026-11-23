using System;
using System.Diagnostics;
using System.Globalization;
using System.Runtime.InteropServices;
using System.Text;

internal sealed class ParentWindow : IDisposable
{
    private readonly Process owner;
    private readonly IntPtr window;
    private readonly IntPtr sender;

    private ParentWindow(Process owner, IntPtr window)
    {
        this.owner = owner;
        this.window = window;
        using (var current = Process.GetCurrentProcess()) sender = new IntPtr(current.Id);
    }

    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetClassName(IntPtr hwnd, StringBuilder name, int max);
    [DllImport("user32.dll")]
    private static extern bool PostMessage(IntPtr hwnd, uint msg, UIntPtr state, IntPtr processId);

    public static ParentWindow Open(string[] args)
    {
        int pid;
        ulong handle;
        if (args.Length != 4 || args[0] != "--watch" || (args[3] != "1" && args[3] != "0") ||
            !int.TryParse(args[1], NumberStyles.None, CultureInfo.InvariantCulture, out pid) || pid <= 0 ||
            !ulong.TryParse(args[2], NumberStyles.None, CultureInfo.InvariantCulture, out handle) || handle == 0)
            return null;

        var parent = new ParentWindow(Process.GetProcessById(pid), new IntPtr(unchecked((long)handle)));
        if (parent.Alive) return parent;
        parent.Dispose();
        return null;
    }

    public bool Alive
    {
        get
        {
            uint pid;
            if (owner.HasExited || GetWindowThreadProcessId(window, out pid) == 0 || pid != (uint)owner.Id)
                return false;
            var name = new StringBuilder(80);
            return GetClassName(window, name, name.Capacity) != 0 && name.ToString() == "ArkCatsOverlayWindowClass";
        }
    }

    public bool Report(WatchStatus state)
    {
        return Alive && PostMessage(window, 0x8000 + 41, new UIntPtr((uint)state), sender);
    }

    public void Dispose() { owner.Dispose(); }
}
