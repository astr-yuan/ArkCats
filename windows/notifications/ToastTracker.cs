using System;
using System.Collections.Generic;

internal struct ToastKey : IEquatable<ToastKey>
{
    private readonly uint id;
    private readonly long createdUtcTicks;

    public ToastKey(uint id, long createdUtcTicks)
    {
        this.id = id;
        this.createdUtcTicks = createdUtcTicks;
    }

    public bool Equals(ToastKey other)
    {
        return id == other.id && createdUtcTicks == other.createdUtcTicks;
    }

    public override bool Equals(object value)
    {
        return value is ToastKey && Equals((ToastKey)value);
    }

    public override int GetHashCode()
    {
        return id.GetHashCode() ^ createdUtcTicks.GetHashCode();
    }
}

internal sealed class ToastTracker
{
    private HashSet<ToastKey> lastSnapshot;
    private long lastAnimation = -5000;

    public void Reset()
    {
        lastSnapshot = null;
        lastAnimation = -5000;
    }

    public int Observe(HashSet<ToastKey> current)
    {
        int added = 0;
        if (lastSnapshot != null)
            foreach (var key in current)
                if (!lastSnapshot.Contains(key))
                    added++;
        lastSnapshot = current;
        return added;
    }

    public bool BeginAnimation(long milliseconds)
    {
        if (milliseconds - lastAnimation < 5000)
            return false;
        lastAnimation = milliseconds;
        return true;
    }
}
