#pragma once
#include <windows.h>
#include <wtsapi32.h>
#include <cstdint>
#include <string>

// Messages carry lifecycle events only. No notification text crosses this boundary.
constexpr UINT WM_PET_NOTIFICATION = WM_APP + 41;

class NotificationBridge {
public:
    enum class State { Paused, Starting, Ready, Denied, NeedsAccess, Failed, MissingHelper };

    NotificationBridge() = default;
    NotificationBridge(const NotificationBridge&) = delete;
    NotificationBridge& operator=(const NotificationBridge&) = delete;
    ~NotificationBridge() { stop(); }

    State state() const { return state_; }
    bool canRetry() const {
        return state_ == State::Denied || state_ == State::NeedsAccess ||
               state_ == State::Failed || state_ == State::MissingHelper;
    }
    const wchar_t* statusText() const {
        switch (state_) {
            case State::Paused:        return L"通知反应已暂停";
            case State::Starting:      return L"正在检查通知权限并读取当前通知列表…";
            case State::Ready:         return L"已就绪，等待新通知";
            case State::Denied:        return L"通知权限已拒绝或撤销，请在 Windows 设置中允许通知访问";
            case State::NeedsAccess:   return L"尚未获得通知访问权限";
            case State::MissingHelper: return L"找不到 ChristineNotifications.exe，请完整解压发布包";
            case State::Failed:        return L"通知监听已停止，可重试连接";
        }
        return L"";
    }

    void start(HWND owner, bool requestPermission) {
        stop();
        std::wstring path(32768, L'\0');
        DWORD len = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        state_ = State::Failed;
        if (!len || len >= path.size()) return;
        path.resize(len);
        auto slash = path.find_last_of(L"\\/");
        if (slash == std::wstring::npos) return;
        path.replace(slash + 1, std::wstring::npos, L"ChristineNotifications.exe");
        std::wstring command = L"\"" + path + L"\" --watch " + std::to_wstring(GetCurrentProcessId()) +
            L" " + std::to_wstring(reinterpret_cast<std::uintptr_t>(owner)) +
            (requestPermission ? L" 1" : L" 0");
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION child{};
        if (!CreateProcessW(path.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                            nullptr, nullptr, &startup, &child)) {
            DWORD error = GetLastError();
            if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) state_ = State::MissingHelper;
            return;
        }
        CloseHandle(child.hThread);
        process_ = child.hProcess;
        owner_ = owner;
        state_ = State::Starting;
    }

    void stop() {
        if (process_) {
            if (WaitForSingleObject(process_, 0) == WAIT_TIMEOUT) {
                TerminateProcess(process_, 0);
                WaitForSingleObject(process_, 1000);
            }
            CloseHandle(process_);
            process_ = nullptr;
        }
        if (owner_) {
            MSG old{};
            while (PeekMessageW(&old, owner_, WM_PET_NOTIFICATION, WM_PET_NOTIFICATION, PM_REMOVE)) {}
            owner_ = nullptr;
        }
        state_ = State::Paused;
    }

    // Terminal states also travel as process exit codes, so a fast-exiting
    // helper cannot lose its final status to a race with the UI timer.
    void poll() {
        if (!process_ || WaitForSingleObject(process_, 0) != WAIT_OBJECT_0) return;
        DWORD exitCode = 4;
        GetExitCodeProcess(process_, &exitCode);
        if (!canRetry()) state_ = terminalState(exitCode);
        CloseHandle(process_);
        process_ = nullptr;
    }

    bool receive(WPARAM code, LPARAM source) {
        poll();
        if (!process_ || source != static_cast<LPARAM>(GetProcessId(process_))) return false;
        if (code == 1 && state_ == State::Starting) state_ = State::Ready;
        else if (code >= 3 && code <= 5) state_ = terminalState(static_cast<DWORD>(code));
        else if (code == 2 && state_ == State::Ready) return true;
        return false;
    }

    // Cover startup on a locked/disconnected desktop and missed WTS broadcasts.
    // On Windows a disconnected session can still have a named input desktop.
    static bool hasInteractiveDesktop() {
        LPWSTR info = nullptr;
        DWORD bytes = 0;
        if (!WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION,
                                         WTSConnectState, &info, &bytes)) return false;
        bool active = bytes >= sizeof(WTS_CONNECTSTATE_CLASS) &&
                      *reinterpret_cast<const WTS_CONNECTSTATE_CLASS*>(info) == WTSActive;
        WTSFreeMemory(info);
        if (!active) return false;
        HDESK input = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
        if (!input) return false;
        wchar_t inputName[256]{}, windowName[256]{};
        HDESK window = GetThreadDesktop(GetCurrentThreadId());
        bool sameDesktop =
            GetUserObjectInformationW(input, UOI_NAME, inputName, sizeof(inputName), &bytes) &&
            GetUserObjectInformationW(window, UOI_NAME, windowName, sizeof(windowName), &bytes) &&
            _wcsicmp(inputName, windowName) == 0;
        CloseDesktop(input);
        return sameDesktop;
    }

private:
    static State terminalState(DWORD code) {
        if (code == 3) return State::Denied;
        if (code == 5) return State::NeedsAccess;
        return State::Failed;
    }
    HANDLE process_ = nullptr;
    HWND owner_ = nullptr;
    State state_ = State::Paused;
};
