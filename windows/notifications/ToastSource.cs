using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using Windows.Foundation;
using Windows.UI.Notifications;
using Windows.UI.Notifications.Management;

internal sealed class ToastSource
{
    private readonly UserNotificationListener listener = UserNotificationListener.Current;

    public Task<UserNotificationListenerAccessStatus> RequestAccess()
    {
        return WaitForResult(listener.RequestAccessAsync(), CancellationToken.None);
    }

    public UserNotificationListenerAccessStatus Access
    {
        get { return listener.GetAccessStatus(); }
    }

    public async Task<HashSet<ToastKey>> Snapshot(CancellationToken token)
    {
        var list = await WaitForResult(listener.GetNotificationsAsync(NotificationKinds.Toast), token);
        token.ThrowIfCancellationRequested();
        var keys = new HashSet<ToastKey>();
        foreach (var item in list)
            keys.Add(new ToastKey(item.Id, item.CreationTime.UtcDateTime.Ticks));
        return keys;
    }

    private static async Task<T> WaitForResult<T>(IAsyncOperation<T> operation, CancellationToken token)
    {
        var result = new TaskCompletionSource<T>(TaskCreationOptions.RunContinuationsAsynchronously);
        operation.Completed = delegate(IAsyncOperation<T> completed, AsyncStatus status)
        {
            try
            {
                if (status == AsyncStatus.Canceled)
                    result.TrySetCanceled();
                else
                    result.TrySetResult(completed.GetResults());
            }
            catch (Exception e) { result.TrySetException(e); }
            finally { try { completed.Close(); } catch (Exception) { } }
        };
        using (token.Register(delegate {
            try { operation.Cancel(); } catch (Exception) { }
            result.TrySetCanceled();
        }))
            return await result.Task;
    }
}
