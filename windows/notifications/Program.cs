using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using Windows.UI.Notifications.Management;

[assembly: System.Reflection.AssemblyTitle("ArkCats · Windows 通知助手")]
[assembly: System.Reflection.AssemblyProduct("ArkCats")]
[assembly: System.Reflection.AssemblyVersion("2.0.0.0")]
[assembly: System.Reflection.AssemblyFileVersion("2.0.0.0")]

internal enum WatchStatus { Ready = 1, Added = 2, Denied = 3, Failed = 4, NotGranted = 5 }

internal static class Program
{
    [STAThread]
    private static int Main(string[] args)
    {
        if (args.Length == 1 && args[0] == "--self-test") return SelfTest();
        try
        {
            using (var parent = ParentWindow.Open(args))
            {
                if (parent == null) return 0;
                SynchronizationContext.SetSynchronizationContext(new WindowsFormsSynchronizationContext());
                using (var watch = new WatchContext(parent, args[3] == "1"))
                {
                    Application.Run(watch);
                    return watch.Result;
                }
            }
        }
        catch (Exception) { return (int)WatchStatus.Failed; }
    }

    private static int SelfTest()
    {
        var old = new ToastKey(7, 100);
        var next = new ToastKey(7, 101);
        var third = new ToastKey(8, 102);
        var tracker = new ToastTracker();
        if (tracker.Observe(new HashSet<ToastKey> {old}) != 0) return 1;
        if (tracker.Observe(new HashSet<ToastKey> {old, next, third}) != 2) return 2;
        if (tracker.Observe(new HashSet<ToastKey> {next, third}) != 0) return 3;
        if (!tracker.BeginAnimation(100) || tracker.BeginAnimation(5099) || !tracker.BeginAnimation(5100)) return 4;
        tracker.Reset();
        if (tracker.Observe(new HashSet<ToastKey> {old, third}) != 0) return 5;
        if (WatchContext.MissingPermission(false, UserNotificationListenerAccessStatus.Unspecified) != WatchStatus.NotGranted ||
            WatchContext.MissingPermission(true, UserNotificationListenerAccessStatus.Unspecified) != WatchStatus.Denied ||
            WatchContext.MissingPermission(false, UserNotificationListenerAccessStatus.Denied) != WatchStatus.Denied) return 6;
        return 0;
    }
}

internal sealed class WatchContext : ApplicationContext
{
    private readonly ParentWindow parent;
    private readonly ToastTracker tracker = new ToastTracker();
    private readonly SessionWindow session = new SessionWindow();
    private readonly System.Windows.Forms.Timer timer = new System.Windows.Forms.Timer { Interval = 850 };
    private readonly Stopwatch clock = Stopwatch.StartNew();
    private readonly bool explicitStart;
    private ToastSource source;
    private CancellationTokenSource read;
    private int epoch;
    private long lastTick;
    private bool active, busy, done, attempted, hadConsent, baseline = true;
    public int Result { get; private set; }

    public WatchContext(ParentWindow parent, bool explicitStart)
    {
        this.parent = parent;
        this.explicitStart = explicitStart;
        session.Changed += Reset;
        timer.Tick += async delegate {
            try { await Tick(); }
            catch (Exception) { Finish(WatchStatus.Failed); }
        };
        timer.Start();
    }

    private void Reset()
    {
        epoch++;
        baseline = true;
        tracker.Reset();
        if (read != null) read.Cancel();
    }

    private void Finish(WatchStatus? terminal)
    {
        if (done) return;
        done = true;
        timer.Stop();
        try
        {
            if (terminal.HasValue)
            {
                Result = (int)terminal.Value;
                // The parent can also recover this exact status from our process exit code.
                parent.Report(terminal.Value);
            }
            if (read != null) read.Cancel();
        }
        finally { ExitThread(); }
    }

    internal static WatchStatus MissingPermission(bool previouslyAllowed, UserNotificationListenerAccessStatus current)
    {
        return previouslyAllowed || current == UserNotificationListenerAccessStatus.Denied
            ? WatchStatus.Denied : WatchStatus.NotGranted;
    }

    private async Task Tick()
    {
        if (done) return;
        if (!parent.Alive) { Finish(null); return; }
        long now = clock.ElapsedMilliseconds;
        bool enabled = session.Active();
        if (enabled != active || (lastTick != 0 && now - lastTick > 3500)) Reset();
        active = enabled;
        lastTick = now;
        if (!active || busy) return;

        int currentEpoch = epoch;
        busy = true;
        try
        {
            if (source == null) source = new ToastSource();
            var access = source.Access;
            if (access != UserNotificationListenerAccessStatus.Allowed && explicitStart && !attempted && !hadConsent)
            {
                attempted = true;
                access = await source.RequestAccess();
            }
            if (done || !parent.Alive) { Finish(null); return; }
            if (access != UserNotificationListenerAccessStatus.Allowed)
            {
                Finish(MissingPermission(hadConsent, access));
                return;
            }
            hadConsent = true;
            if (currentEpoch != epoch || !active) return;

            read = new CancellationTokenSource(8000);
            var current = await source.Snapshot(read.Token);
            if (done || currentEpoch != epoch) return;
            if (!parent.Alive) { Finish(null); return; }
            if (source.Access != UserNotificationListenerAccessStatus.Allowed) { Finish(WatchStatus.Denied); return; }
            if (!session.Active()) { Reset(); active = false; return; }
            if (clock.ElapsedMilliseconds - now > 3500) { tracker.Reset(); baseline = true; }
            int added = tracker.Observe(current);
            if (baseline)
            {
                baseline = false;
                if (!parent.Report(WatchStatus.Ready)) Finish(null);
            }
            else if (added > 0 && tracker.BeginAnimation(clock.ElapsedMilliseconds))
            {
                if (!parent.Report(WatchStatus.Added)) Finish(null);
            }
        }
        catch (OperationCanceledException)
        {
            if (!done && currentEpoch == epoch) Finish(WatchStatus.Failed);
        }
        finally
        {
            if (read != null) { read.Dispose(); read = null; }
            busy = false;
        }
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing) { timer.Dispose(); session.Dispose(); }
        base.Dispose(disposing);
    }
}
