#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <shellapi.h>
#include <objidl.h>
#include <gdiplus.h>
#include <string>
#include <cmath>
#include <algorithm>
#include "KeyMap.hpp"
#include "InputEngine.hpp"
#include "CatPhysics.hpp"
#include "CatRenderer.hpp"
#include "NotificationBridge.hpp"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

namespace {

constexpr wchar_t kPetName[] = L"Miss. Christine";
constexpr UINT WM_TRAYICON = WM_APP + 1;
constexpr UINT WM_WAKE_EXISTING = WM_APP + 2;
constexpr UINT_PTR TIMER_RENDER = 1;
constexpr UINT_PTR TIMER_NOTIFICATIONS = 2;
constexpr int HOTKEY_CLICKTHROUGH_ID = 101;
UINT g_msgTaskbarCreated = 0;

enum MenuCmd : UINT {
    CMD_TOGGLE_VISIBLE = 1001,
    CMD_TOGGLE_CLICKTHROUGH,
    CMD_TOGGLE_TOPMOST,
    CMD_TOGGLE_AUTOSTART,
    CMD_TOGGLE_BADGES,
    CMD_TOGGLE_FOOT,
    CMD_TOGGLE_PET,
    CMD_COAT_BLACK,
    CMD_COAT_SIAMESE,
    CMD_COAT_SHORTHAIR,
    CMD_COAT_ORANGE_WHITE,
    CMD_COAT_SAPPHIRE_TWIN_TAIL,
    CMD_KB_MILK,
    CMD_KB_HACKER,
    CMD_KB_RETRO,
    CMD_EYE_LIME,
    CMD_EYE_BLUE,
    CMD_EYE_AMBER,
    CMD_EYE_CYAN,
    CMD_EYE_SOFT_AMBER,
    CMD_EYE_REFERENCE_HAZEL,
    CMD_SCALE_65,
    CMD_SCALE_85,
    CMD_SCALE_100,
    CMD_SCALE_125,
    CMD_SCALE_150,
    CMD_DEMO_BURST,
    CMD_RESET_POS,
    CMD_QUIT,
    CMD_TOGGLE_NOTIFICATIONS,
    CMD_RETRY_NOTIFICATIONS
};

struct AppState {
    HWND hwnd = nullptr;
    HWND menuOwnerHwnd = nullptr; // Non-NOACTIVATE helper window so TrackPopupMenu from tray NEVER fails!
    HICON trayIcon = nullptr;
    NOTIFYICONDATAW nid = {};

    CatSettings settings;
    InputEngine input;
    CatPhysics physics;
    NotificationBridge notifications;
    bool sessionUnavailable = false;
    bool suspended = false;
    bool requestAccessOnShow = false;
    bool sessionEventsRegistered = false;

    bool isVisible = true;
    bool isDragging = false;
    POINT dragStartCursor = {};
    POINT dragStartWinOrigin = {};
    bool hasLastHover = false;
    Pt lastHoverCanvas = {};

    // Reusable 32-bit premultiplied DIB backing buffer
    HDC memDC = nullptr;
    HBITMAP dibBmp = nullptr;
    HBITMAP oldBmp = nullptr;
    void* dibBits = nullptr;
    int bufW = 0;
    int bufH = 0;

    double lastTickTime = 0.0;
    bool demoActive = false;
    double demoStartTime = 0.0;
    int demoStepIdx = 0;

    void destroyBuffer() {
        if (memDC && oldBmp) SelectObject(memDC, oldBmp);
        if (dibBmp) DeleteObject(dibBmp);
        if (memDC) DeleteDC(memDC);
        memDC = nullptr; dibBmp = nullptr; oldBmp = nullptr; dibBits = nullptr;
        bufW = 0; bufH = 0;
    }

    void ensureBuffer(int w, int h) {
        if (w == bufW && h == bufH && memDC && dibBits) return;
        destroyBuffer();

        HDC screenDC = GetDC(nullptr);
        memDC = CreateCompatibleDC(screenDC);
        ReleaseDC(nullptr, screenDC);

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = w;
        bmi.bmiHeader.biHeight = -h; // top-down 32bpp DIB
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        dibBmp = CreateDIBSection(memDC, &bmi, DIB_RGB_COLORS, &dibBits, nullptr, 0);
        if (dibBmp) {
            oldBmp = (HBITMAP)SelectObject(memDC, dibBmp);
            bufW = w;
            bufH = h;
        }
    }
};

AppState* g_app = nullptr;

HICON createCatTrayIcon();

// MARK: - Windows Auto-Start (HKCU\Software\Microsoft\Windows\CurrentVersion\Run)

std::wstring getCurrentExeQuotedPath() {
    wchar_t buf[MAX_PATH * 2] = {};
    DWORD len = GetModuleFileNameW(nullptr, buf, (DWORD)(sizeof(buf) / sizeof(buf[0]) - 1));
    if (len == 0) return L"";
    return L"\"" + std::wstring(buf, len) + L"\"";
}

bool isAutoStartEnabled() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false;
    }
    DWORD type = 0, size = 0;
    bool exists = (RegQueryValueExW(hKey, L"KuroCat", nullptr, &type, nullptr, &size) == ERROR_SUCCESS && size > 0);
    RegCloseKey(hKey);
    return exists;
}

bool setAutoStartEnabled(bool enable) {
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) != ERROR_SUCCESS) {
        return false;
    }
    bool ok = false;
    if (enable) {
        std::wstring cmd = getCurrentExeQuotedPath();
        if (!cmd.empty()) {
            ok = (RegSetValueExW(hKey, L"KuroCat", 0, REG_SZ, (const BYTE*)cmd.c_str(), (DWORD)((cmd.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS);
        }
    } else {
        LSTATUS st = RegDeleteValueW(hKey, L"KuroCat");
        ok = (st == ERROR_SUCCESS || st == ERROR_FILE_NOT_FOUND);
    }
    RegCloseKey(hKey);
    return ok;
}

void showTrayBalloon(AppState& app, const wchar_t* title, const wchar_t* body) {
    NOTIFYICONDATAW b = app.nid;
    b.uFlags = NIF_INFO;
    b.dwInfoFlags = NIIF_INFO | NIIF_NOSOUND;
    wcsncpy_s(b.szInfoTitle, title, _TRUNCATE);
    wcsncpy_s(b.szInfo, body, _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &b);
}

void registerTrayIcon(AppState& app) {
    if (!app.trayIcon) app.trayIcon = createCatTrayIcon();
    app.nid = {};
    app.nid.cbSize = sizeof(app.nid);
    app.nid.hWnd = app.menuOwnerHwnd ? app.menuOwnerHwnd : app.hwnd;
    app.nid.uID = 1;
    app.nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    app.nid.uCallbackMessage = WM_TRAYICON;
    app.nid.hIcon = app.trayIcon;
    std::wstring tooltip = std::wstring(kPetName) + L" · 点击打开桌宠菜单";
    wcscpy_s(app.nid.szTip, tooltip.c_str());
    Shell_NotifyIconW(NIM_ADD, &app.nid);
}

HICON createCatTrayIcon() {
    using namespace Gdiplus;
    constexpr int sz = 32;
    Bitmap bmp(sz, sz, PixelFormat32bppPARGB);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.Clear(Color(0, 0, 0, 0));

    SolidBrush black(Color(255, 22, 22, 26));
    PointF lEar[3] = { {4, 15}, {6, 3}, {14, 11} };
    PointF rEar[3] = { {28, 15}, {26, 3}, {18, 11} };
    g.FillPolygon(&black, lEar, 3);
    g.FillPolygon(&black, rEar, 3);
    g.FillEllipse(&black, 4.0f, 8.0f, 24.0f, 21.0f);

    SolidBrush blue(Color(255, 94, 199, 250));
    g.FillEllipse(&blue, 8.5f, 14.5f, 6.0f, 5.2f);
    g.FillEllipse(&blue, 17.5f, 14.5f, 6.0f, 5.2f);
    g.FillEllipse(&black, 10.7f, 15.2f, 1.8f, 3.8f);
    g.FillEllipse(&black, 19.7f, 15.2f, 1.8f, 3.8f);

    SolidBrush red(Color(255, 225, 43, 61));
    PointF bowLeft[] = {{8, 22}, {16, 25}, {8, 29}};
    PointF bowRight[] = {{24, 22}, {24, 29}, {16, 25}};
    g.FillPolygon(&red, bowLeft, 3);
    g.FillPolygon(&red, bowRight, 3);
    g.FillEllipse(&blue, 14.0f, 23.0f, 4.0f, 5.0f);

    HICON hIcon = nullptr;
    bmp.GetHICON(&hIcon);
    return hIcon;
}

UINT windowDpi(HWND hwnd) {
    using GetWindowDpiFn = UINT (WINAPI*)(HWND);
    static const auto getWindowDpi = reinterpret_cast<GetWindowDpiFn>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
    if (getWindowDpi) {
        UINT dpi = getWindowDpi(hwnd);
        if (dpi) return dpi;
    }
    HDC dc = GetDC(hwnd);
    int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96;
    if (dc) ReleaseDC(hwnd, dc);
    return dpi > 0 ? static_cast<UINT>(dpi) : 96;
}

SIZE petWindowSize(float scale, UINT dpi) {
    float factor = scale * dpi / 96.0f;
    return {std::lround(CatPhysics::CanvasWidth * factor),
            std::lround(CatPhysics::CanvasHeight * factor)};
}

RECT monitorWorkArea(HMONITOR monitor) {
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, &info)) return info.rcWork;
    RECT fallback = {};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &fallback, 0);
    return fallback;
}

POINT fitToWorkArea(POINT origin, SIZE size, RECT work) {
    return {std::clamp(origin.x, work.left, std::max(work.left, work.right - size.cx)),
            std::clamp(origin.y, work.top, std::max(work.top, work.bottom - size.cy))};
}

void applyWindowScaleAndPosition(AppState& app, bool anchorBottomCenter) {
    RECT previous = {};
    GetWindowRect(app.hwnd, &previous);
    HMONITOR current = MonitorFromWindow(app.hwnd, MONITOR_DEFAULTTONEAREST);
    bool defaultPosition = app.settings.posX == -99999 || app.settings.posY == -99999;
    HMONITOR target = defaultPosition
        ? MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY)
        : current;
    RECT work = monitorWorkArea(target);
    if (current != target) {
        // Move to the target monitor before querying its effective window DPI.
        SetWindowPos(app.hwnd, nullptr, work.left, work.top, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    UINT dpi = windowDpi(app.hwnd);
    SIZE size = petWindowSize(app.settings.scale, dpi);
    POINT origin = {app.settings.posX, app.settings.posY};
    if (defaultPosition) {
        origin = {work.right - size.cx - MulDiv(24, dpi, 96),
                  work.bottom - size.cy - MulDiv(12, dpi, 96)};
    } else if (anchorBottomCenter) {
        origin = {previous.left + (previous.right - previous.left - size.cx) / 2,
                  previous.bottom - size.cy};
    }
    origin = fitToWorkArea(origin, size, work);
    HWND insertAfter = app.settings.alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST;
    SetWindowPos(app.hwnd, insertAfter, origin.x, origin.y, size.cx, size.cy, SWP_NOACTIVATE);
    app.settings.posX = origin.x;
    app.settings.posY = origin.y;
    app.settings.saveToRegistry();
}

void setClickThrough(HWND hwnd, bool transparent) {
    LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    bool isTrans = (ex & WS_EX_TRANSPARENT) != 0;
    if (isTrans != transparent) {
        if (transparent) ex |= WS_EX_TRANSPARENT;
        else ex &= ~WS_EX_TRANSPARENT;
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex);
    }
}

void refreshNotifications(AppState& app) {
    if (!app.settings.notificationReactions) return;
    if (!app.isVisible || app.suspended || app.sessionUnavailable ||
        !NotificationBridge::hasInteractiveDesktop()) {
        app.notifications.stop();
        return;
    }
    app.notifications.poll();
    if (app.notifications.state() == NotificationBridge::State::Paused) {
        app.notifications.start(app.hwnd, app.requestAccessOnShow);
        app.requestAccessOnShow = false;
    }
}

void setPetVisible(AppState& app, bool visible) {
    app.isVisible = visible;
    app.hasLastHover = false;
    app.demoActive = false;
    if (visible) {
        // Start with the current pointer/button state after a pause.
        GetCursorPos(&app.input.lastCursorPos);
        app.input.rawDeltaAccumX = app.input.rawDeltaAccumY = 0.0f;
        app.input.mouseVelocity = {};
        app.physics.mousePressLeft.target = app.input.leftMouseDown ? 1.0f : 0.0f;
        app.physics.mousePressRight.target = app.input.rightMouseDown ? 1.0f : 0.0f;
        app.lastTickTime = nowSeconds();
        ShowWindow(app.hwnd, SW_SHOWNOACTIVATE);
        SetTimer(app.hwnd, TIMER_RENDER, 16, nullptr);
    } else {
        KillTimer(app.hwnd, TIMER_RENDER);
        app.physics.floatingKeycaps.clear();
        ShowWindow(app.hwnd, SW_HIDE);
    }
    refreshNotifications(app);
}

void showContextMenu(AppState& app, POINT screenPt) {
    refreshNotifications(app);
    HMENU menu = CreatePopupMenu();

    // Status & live stats header
    std::wstring statusStr = app.input.isRawInputRegistered
        ? L"\u25CF \u786C\u4EF6\u7EA7\u5168\u72B6\u6001\u76D1\u542C\u4E2D (Raw Input \u6E38\u620F/\u6253\u5B57)"
        : L"\u26A0 Raw Input \u672A\u542F\u52A8";
    AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, statusStr.c_str());

    std::wstring stats = L"本次运行击键 " + std::to_wstring(app.input.sessionKeystrokes)
        + L"  \u00B7  KPM " + std::to_wstring(app.input.currentKPM())
        + L"  \u00B7  \u6478\u732B " + std::to_wstring(app.physics.totalPetSessions) + L" \u6B21";
    AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, stats.c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(menu, MF_STRING, CMD_TOGGLE_VISIBLE, app.isVisible ? L"\u9690\u85CF\u5C0F\u732B" : L"\u663E\u793A\u5C0F\u732B");
    AppendMenuW(menu, MF_STRING | (app.settings.fullClickThrough ? MF_CHECKED : 0), CMD_TOGGLE_CLICKTHROUGH, L"\u5B8C\u5168\u9F20\u6807\u7A7F\u900F (Ctrl+Alt+Shift+P / \u6309\u4F4FAlt\u53EF\u53F3\u952E)");
    AppendMenuW(menu, MF_STRING | (app.settings.alwaysOnTop ? MF_CHECKED : 0), CMD_TOGGLE_TOPMOST, L"\u7A97\u53E3\u7F6E\u9876\u663E\u793A");
    AppendMenuW(menu, MF_STRING | (isAutoStartEnabled() ? MF_CHECKED : 0), CMD_TOGGLE_AUTOSTART, L"\u5F00\u673A\u81EA\u52A8\u542F\u52A8");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(menu, MF_STRING | (app.settings.notificationReactions ? MF_CHECKED : 0),
                CMD_TOGGLE_NOTIFICATIONS, L"收到 Windows 通知时做出反应");
    if (app.settings.notificationReactions) {
        AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, app.notifications.statusText());
        if (app.notifications.canRetry()) {
            AppendMenuW(menu, MF_STRING, CMD_RETRY_NOTIFICATIONS, L"重新连接并检查通知权限");
        }
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(menu, MF_STRING | (app.settings.showKeyBadges ? MF_CHECKED : 0), CMD_TOGGLE_BADGES, L"\u663E\u793A\u6309\u952E\u6C14\u6CE1 ([S] [W] [\u21E7])");
    AppendMenuW(menu, MF_STRING | (app.settings.footEasterEgg ? MF_CHECKED : 0), CMD_TOGGLE_FOOT, L"3\u952E\u540C\u65F6\u6309\u300C\u7528\u811A\u6309\u300D\u5F69\u86CB");
    AppendMenuW(menu, MF_STRING | (app.settings.petInteraction ? MF_CHECKED : 0), CMD_TOGGLE_PET, L"\u6478\u5934/\u6478\u80CC\u95ED\u773C\u4E92\u52A8");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    // Cat Coat Skin submenu
    HMENU coatMenu = CreatePopupMenu();
    std::wstring twinTailLabel = std::wstring(kPetName) + L"（双尾蓝宝石黑猫）";
    AppendMenuW(coatMenu, MF_STRING | (app.settings.coatTheme == CatCoatTheme::SapphireTwinTail ? MF_CHECKED : 0), CMD_COAT_SAPPHIRE_TWIN_TAIL, twinTailLabel.c_str());
    AppendMenuW(coatMenu, MF_STRING | (app.settings.coatTheme == CatCoatTheme::ClassicBlack ? MF_CHECKED : 0), CMD_COAT_BLACK, L"\u7ECF\u5178\u5C0F\u9ED1\u732B (\u539F\u7248)");
    AppendMenuW(coatMenu, MF_STRING | (app.settings.coatTheme == CatCoatTheme::Siamese ? MF_CHECKED : 0), CMD_COAT_SIAMESE, L"\u6316\u7164\u66B9\u7F57\u732B");
    AppendMenuW(coatMenu, MF_STRING | (app.settings.coatTheme == CatCoatTheme::Shorthair ? MF_CHECKED : 0), CMD_COAT_SHORTHAIR, L"美短（银虎斑加白）");
    AppendMenuW(coatMenu, MF_STRING | (app.settings.coatTheme == CatCoatTheme::OrangeWhite ? MF_CHECKED : 0), CMD_COAT_ORANGE_WHITE, L"橘白猫");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)coatMenu, L"\u732B\u54AA\u76AE\u80A4");

    // Keyboard Theme submenu
    HMENU kbMenu = CreatePopupMenu();
    AppendMenuW(kbMenu, MF_STRING | (app.settings.keyboardTheme == KeyboardTheme::ClassicMilk ? MF_CHECKED : 0), CMD_KB_MILK, L"\u7ECF\u5178\u5976\u767D\u6696\u9EC4 (\u539F\u7248)");
    AppendMenuW(kbMenu, MF_STRING | (app.settings.keyboardTheme == KeyboardTheme::MidnightHacker ? MF_CHECKED : 0), CMD_KB_HACKER, L"\u6697\u591C\u6781\u5BA2\u9ED1");
    AppendMenuW(kbMenu, MF_STRING | (app.settings.keyboardTheme == KeyboardTheme::RetroCream ? MF_CHECKED : 0), CMD_KB_RETRO, L"\u590D\u53E4\u5976\u6CB9\u7070");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)kbMenu, L"\u952E\u76D8\u914D\u8272");

    // Eye Color submenu
    HMENU eyeMenu = CreatePopupMenu();
    AppendMenuW(eyeMenu, MF_STRING | (app.settings.eyeTheme == EyeColorTheme::NeonChartreuse ? MF_CHECKED : 0), CMD_EYE_LIME, L"\u7ECF\u5178\u8367\u5149\u9EC4\u7EFF (\u539F\u7248)");
    AppendMenuW(eyeMenu, MF_STRING | (app.settings.eyeTheme == EyeColorTheme::SiameseBlue ? MF_CHECKED : 0), CMD_EYE_BLUE, L"\u66B9\u7F57\u5B9D\u77F3\u84DD");
    AppendMenuW(eyeMenu, MF_STRING | (app.settings.eyeTheme == EyeColorTheme::WarmAmber ? MF_CHECKED : 0), CMD_EYE_AMBER, L"\u7425\u73C0\u91D1\u77B3");
    AppendMenuW(eyeMenu, MF_STRING | (app.settings.eyeTheme == EyeColorTheme::IceCyan ? MF_CHECKED : 0), CMD_EYE_CYAN, L"\u51B0\u6676\u6D45\u84DD");
    AppendMenuW(eyeMenu, MF_STRING | (app.settings.eyeTheme == EyeColorTheme::SoftAmber ? MF_CHECKED : 0), CMD_EYE_SOFT_AMBER, L"柔和琥珀");
    AppendMenuW(eyeMenu, MF_STRING | (app.settings.eyeTheme == EyeColorTheme::ReferenceHazel ? MF_CHECKED : 0), CMD_EYE_REFERENCE_HAZEL, L"淡金青绿");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)eyeMenu, L"\u77B3\u8272\u5207\u6362");

    // Scale submenu
    HMENU scaleMenu = CreatePopupMenu();
    auto chk = [&](float v) -> UINT { return std::fabs(app.settings.scale - v) < 0.05f ? MF_CHECKED : 0; };
    AppendMenuW(scaleMenu, MF_STRING | chk(0.65f), CMD_SCALE_65,  L"\u8FF7\u4F60 (0.65x)");
    AppendMenuW(scaleMenu, MF_STRING | chk(0.85f), CMD_SCALE_85,  L"\u504F\u5C0F (0.85x)");
    AppendMenuW(scaleMenu, MF_STRING | chk(1.00f), CMD_SCALE_100, L"\u6807\u51C6 (1.0x)");
    AppendMenuW(scaleMenu, MF_STRING | chk(1.25f), CMD_SCALE_125, L"\u504F\u5927 (1.25x)");
    AppendMenuW(scaleMenu, MF_STRING | chk(1.50f), CMD_SCALE_150, L"\u8D85\u5927 (1.5x)");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)scaleMenu, L"\u5C3A\u5BF8\u5927\u5C0F");

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, CMD_DEMO_BURST, L"\u6F14\u793A\u6253\u5B57\u52A8\u753B");
    AppendMenuW(menu, MF_STRING, CMD_RESET_POS, L"\u91CD\u7F6E\u4F4D\u7F6E\u5230\u53F3\u4E0B\u89D2");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, CMD_QUIT, L"\u9000\u51FA KuroCat");

    HWND menuOwner = app.menuOwnerHwnd ? app.menuOwnerHwnd : app.hwnd;
    SetForegroundWindow(menuOwner);
    UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, screenPt.x, screenPt.y, 0, menuOwner, nullptr);
    PostMessageW(menuOwner, WM_NULL, 0, 0);
    DestroyMenu(menu);

    switch (cmd) {
        case CMD_TOGGLE_NOTIFICATIONS:
            app.settings.notificationReactions = !app.settings.notificationReactions;
            app.settings.saveToRegistry();
            app.requestAccessOnShow = app.settings.notificationReactions;
            if (app.settings.notificationReactions) {
                SetTimer(app.hwnd, TIMER_NOTIFICATIONS, 1000, nullptr);
                refreshNotifications(app);
            } else {
                KillTimer(app.hwnd, TIMER_NOTIFICATIONS);
                app.notifications.stop();
            }
            break;
        case CMD_RETRY_NOTIFICATIONS:
            app.notifications.stop();
            app.requestAccessOnShow = true;
            refreshNotifications(app);
            break;
        case CMD_TOGGLE_VISIBLE:
            setPetVisible(app, !app.isVisible);
            break;
        case CMD_TOGGLE_CLICKTHROUGH:
            app.settings.fullClickThrough = !app.settings.fullClickThrough;
            app.settings.saveToRegistry();
            if (app.settings.fullClickThrough) {
                showTrayBalloon(app,
                    L"KuroCat \u5DF2\u5F00\u542F\u5B8C\u5168\u9F20\u6807\u7A7F\u900F",
                    L"\u53D6\u6D88\u65B9\u5F0F\uFF1A\u6309 Ctrl+Alt+Shift+P\uFF0C\u6216\u6309\u4F4F Alt \u952E\u53F3\u952E\u5C0F\u732B\uFF0C\u6216\u70B9\u53F3\u4E0B\u89D2\u6258\u76D8\u5C0F\u732B\u56FE\u6807\uFF0C\u6216\u518D\u6B21\u53CC\u51FB KuroCat.exe");
            }
            break;
        case CMD_TOGGLE_TOPMOST:
            app.settings.alwaysOnTop = !app.settings.alwaysOnTop;
            applyWindowScaleAndPosition(app, false);
            break;
        case CMD_TOGGLE_AUTOSTART:
            setAutoStartEnabled(!isAutoStartEnabled());
            break;
        case CMD_TOGGLE_BADGES:
            app.settings.showKeyBadges = !app.settings.showKeyBadges;
            app.settings.saveToRegistry();
            break;
        case CMD_TOGGLE_FOOT:
            app.settings.footEasterEgg = !app.settings.footEasterEgg;
            app.settings.saveToRegistry();
            break;
        case CMD_TOGGLE_PET:
            app.settings.petInteraction = !app.settings.petInteraction;
            app.settings.saveToRegistry();
            break;
        case CMD_COAT_BLACK:
            app.settings.coatTheme = CatCoatTheme::ClassicBlack;
            app.settings.eyeTheme = EyeColorTheme::NeonChartreuse;
            app.settings.saveToRegistry();
            break;
        case CMD_COAT_SIAMESE:
        case CMD_COAT_SAPPHIRE_TWIN_TAIL:
            app.settings.coatTheme = cmd == CMD_COAT_SIAMESE ? CatCoatTheme::Siamese : CatCoatTheme::SapphireTwinTail;
            app.settings.eyeTheme = EyeColorTheme::SiameseBlue;
            app.settings.saveToRegistry();
            break;
        case CMD_COAT_SHORTHAIR:
        case CMD_COAT_ORANGE_WHITE:
            app.settings.coatTheme = cmd == CMD_COAT_SHORTHAIR ? CatCoatTheme::Shorthair : CatCoatTheme::OrangeWhite;
            app.settings.eyeTheme = EyeColorTheme::SoftAmber;
            app.settings.saveToRegistry();
            break;
        case CMD_EYE_SOFT_AMBER: app.settings.eyeTheme = EyeColorTheme::SoftAmber; app.settings.saveToRegistry(); break;
        case CMD_EYE_REFERENCE_HAZEL: app.settings.eyeTheme = EyeColorTheme::ReferenceHazel; app.settings.saveToRegistry(); break;
        case CMD_KB_MILK:   app.settings.keyboardTheme = KeyboardTheme::ClassicMilk; app.settings.saveToRegistry(); break;
        case CMD_KB_HACKER: app.settings.keyboardTheme = KeyboardTheme::MidnightHacker; app.settings.saveToRegistry(); break;
        case CMD_KB_RETRO:  app.settings.keyboardTheme = KeyboardTheme::RetroCream; app.settings.saveToRegistry(); break;
        case CMD_EYE_LIME:  app.settings.eyeTheme = EyeColorTheme::NeonChartreuse; app.settings.saveToRegistry(); break;
        case CMD_EYE_BLUE:  app.settings.eyeTheme = EyeColorTheme::SiameseBlue; app.settings.saveToRegistry(); break;
        case CMD_EYE_AMBER: app.settings.eyeTheme = EyeColorTheme::WarmAmber; app.settings.saveToRegistry(); break;
        case CMD_EYE_CYAN:  app.settings.eyeTheme = EyeColorTheme::IceCyan; app.settings.saveToRegistry(); break;
        case CMD_SCALE_65:  app.settings.scale = 0.65f; applyWindowScaleAndPosition(app, true); break;
        case CMD_SCALE_85:  app.settings.scale = 0.85f; applyWindowScaleAndPosition(app, true); break;
        case CMD_SCALE_100: app.settings.scale = 1.00f; applyWindowScaleAndPosition(app, true); break;
        case CMD_SCALE_125: app.settings.scale = 1.25f; applyWindowScaleAndPosition(app, true); break;
        case CMD_SCALE_150: app.settings.scale = 1.50f; applyWindowScaleAndPosition(app, true); break;
        case CMD_DEMO_BURST:
            app.demoActive = true;
            app.demoStartTime = nowSeconds();
            app.demoStepIdx = 0;
            break;
        case CMD_RESET_POS:
            app.settings.posX = -99999;
            app.settings.posY = -99999;
            applyWindowScaleAndPosition(app, false);
            break;
        case CMD_QUIT:
            PostMessageW(app.hwnd, WM_CLOSE, 0, 0);
            break;
        default:
            break;
    }
}

void tickAndRender(AppState& app) {
    if (!app.isVisible || !app.hwnd) return;

    double now = nowSeconds();
    float dt = (float)(now - app.lastTickTime);
    app.lastTickTime = now;
    if (dt <= 0.0f || dt > 0.2f) dt = 1.0f / 60.0f;

    // Optional menu demo typing burst
    if (app.demoActive) {
        static const PhysicalKey demoKeys[] = {
            PhysicalKey::W, PhysicalKey::A, PhysicalKey::S, PhysicalKey::D,
            PhysicalKey::F, PhysicalKey::J, PhysicalKey::O, PhysicalKey::E,
            PhysicalKey::Space, PhysicalKey::LeftShift
        };
        int targetIdx = (int)((now - app.demoStartTime) / 0.13);
        while (app.demoStepIdx <= targetIdx && app.demoStepIdx < 10) {
            app.physics.triggerKeyDown(demoKeys[app.demoStepIdx], app.settings);
            app.demoStepIdx++;
        }
        if (app.demoStepIdx >= 10) app.demoActive = false;
    }

    RECT wr = {};
    GetWindowRect(app.hwnd, &wr);
    int w = std::max(1, (int)(wr.right - wr.left));
    int h = std::max(1, (int)(wr.bottom - wr.top));

    app.physics.step(dt, wr, app.input, app.settings);

    // Finish painting before sampling the alpha channel or presenting the DIB.
    app.ensureBuffer(w, h);
    if (!app.memDC || !app.dibBits) return;
    {
        Gdiplus::Bitmap bmp(w, h, w * 4, PixelFormat32bppPARGB, static_cast<BYTE*>(app.dibBits));
        Gdiplus::Graphics g(&bmp);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));
        g.ScaleTransform((float)w / CatPhysics::CanvasWidth, (float)h / CatPhysics::CanvasHeight);
        CatRenderer::draw(g, app.physics, app.settings);
    }

    if (!app.isDragging) {
        // Layered windows pass zero-alpha pixels through automatically, including
        // the gap between the tails. Reserve WS_EX_TRANSPARENT for game mode.
        bool clickThrough = app.settings.fullClickThrough && !(GetAsyncKeyState(VK_MENU) & 0x8000);
        setClickThrough(app.hwnd, clickThrough);
        POINT cur = app.input.lastCursorPos;
        if (!clickThrough && app.settings.petInteraction && PtInRect(&wr, cur)) {
            int x = cur.x - wr.left;
            int y = cur.y - wr.top;
            const auto* pixels = static_cast<const DWORD*>(app.dibBits);
            Pt cp = {
                (float)x / (float)w * CatPhysics::CanvasWidth,
                (float)y / (float)h * CatPhysics::CanvasHeight
            };
            CatHitZone zone = (pixels[static_cast<size_t>(y) * w + x] >> 24) > 16
                ? CatRenderer::hitTest(cp, app.physics) : CatHitZone::None;
            if (zone == CatHitZone::Head || zone == CatHitZone::Body) {
                if (app.hasLastHover) {
                    float d = std::hypot(cp.x - app.lastHoverCanvas.x, cp.y - app.lastHoverCanvas.y);
                    if (d > 1.2f && d < 80.0f) {
                        app.physics.registerPetStroke(cp, d, zone == CatHitZone::Head, app.settings);
                    }
                }
                app.lastHoverCanvas = cp;
                app.hasLastHover = true;
            } else {
                app.hasLastHover = false;
            }
        } else {
            app.hasLastHover = false;
        }
    }

    POINT ptDst = { wr.left, wr.top };
    SIZE szWnd = { w, h };
    POINT ptSrc = { 0, 0 };
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    HDC screenDC = GetDC(nullptr);
    UpdateLayeredWindow(app.hwnd, screenDC, &ptDst, &szWnd, app.memDC, &ptSrc, 0, &blend, ULW_ALPHA);
    ReleaseDC(nullptr, screenDC);
}

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    AppState* app = g_app;
    switch (msg) {
        case WM_INPUT:
            if (app) app->input.handleRawInput(lParam);
            if (GET_RAWINPUT_CODE_WPARAM(wParam) == RIM_INPUT) break;
            return 0;

        case WM_TIMER:
            if (wParam == TIMER_RENDER && app) tickAndRender(*app);
            if (wParam == TIMER_NOTIFICATIONS && app) refreshNotifications(*app);
            return 0;

        case WM_PET_NOTIFICATION:
            if (app && hwnd == app->hwnd && app->settings.notificationReactions &&
                app->notifications.receive(wParam, lParam)) {
                // Rebuild a baseline on return to the desktop. Never replay a queued
                // alert that sat on the UI thread through a suspension or long stall.
                if (!app->isVisible || app->sessionUnavailable || app->suspended ||
                    !NotificationBridge::hasInteractiveDesktop()) {
                    app->notifications.stop();
                } else if (GetTickCount() - static_cast<DWORD>(GetMessageTime()) <= 3000 &&
                           !app->isDragging && app->physics.startleMode() == StartleMode::None) {
                    app->physics.startle(app->input);
                }
            }
            return 0;

        case WM_WTSSESSION_CHANGE:
            if (app && hwnd == app->hwnd) {
                switch (wParam) {
                    case WTS_SESSION_LOCK:
                    case WTS_SESSION_LOGOFF:
                    case WTS_CONSOLE_DISCONNECT:
                    case WTS_REMOTE_DISCONNECT:
                        app->sessionUnavailable = true;
                        break;
                    case WTS_SESSION_UNLOCK:
                    case WTS_SESSION_LOGON:
                    case WTS_CONSOLE_CONNECT:
                    case WTS_REMOTE_CONNECT:
                        app->sessionUnavailable = false;
                        break;
                }
                refreshNotifications(*app);
            }
            return 0;

        case WM_POWERBROADCAST:
            if (app && hwnd == app->hwnd) {
                if (wParam == PBT_APMSUSPEND) app->suspended = true;
                if (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND ||
                    wParam == PBT_APMRESUMECRITICAL) app->suspended = false;
                refreshNotifications(*app);
            }
            return TRUE;

        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT) {
                SetCursor(LoadCursorW(nullptr, IDC_HAND));
                return TRUE;
            }
            break;

        case WM_LBUTTONDOWN: {
            bool altHeld = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
            if (app && (!app->settings.fullClickThrough || altHeld)) {
                app->isDragging = true;
                GetCursorPos(&app->dragStartCursor);
                RECT wr = {};
                GetWindowRect(hwnd, &wr);
                app->dragStartWinOrigin = { wr.left, wr.top };
                SetCapture(hwnd);
            }
            return 0;
        }

        case WM_MOUSEMOVE:
            if (app && app->isDragging) {
                POINT cur = {};
                GetCursorPos(&cur);
                int nx = app->dragStartWinOrigin.x + (cur.x - app->dragStartCursor.x);
                int ny = app->dragStartWinOrigin.y + (cur.y - app->dragStartCursor.y);
                SetWindowPos(hwnd, nullptr, nx, ny, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
            return 0;

        case WM_LBUTTONUP:
        case WM_CANCELMODE:
        case WM_CAPTURECHANGED:
            if (app && app->isDragging) {
                app->isDragging = false;
                if (GetCapture() == hwnd) ReleaseCapture();
                RECT wr = {};
                GetWindowRect(hwnd, &wr);
                app->settings.posX = wr.left;
                app->settings.posY = wr.top;
                applyWindowScaleAndPosition(*app, false);
            }
            break;

        case WM_DPICHANGED:
            if (app && hwnd == app->hwnd) {
                const RECT& suggested = *reinterpret_cast<RECT*>(lParam);
                SIZE size = petWindowSize(app->settings.scale, LOWORD(wParam));
                POINT origin = {suggested.left, suggested.top};
                if (!app->isDragging) {
                    HMONITOR monitor = MonitorFromRect(&suggested, MONITOR_DEFAULTTONEAREST);
                    origin = fitToWorkArea(origin, size, monitorWorkArea(monitor));
                }
                SetWindowPos(hwnd, nullptr, origin.x, origin.y, size.cx, size.cy,
                             SWP_NOZORDER | SWP_NOACTIVATE);
                if (app->isDragging) {
                    GetCursorPos(&app->dragStartCursor);
                    app->dragStartWinOrigin = origin;
                } else {
                    app->settings.posX = origin.x;
                    app->settings.posY = origin.y;
                    app->settings.saveToRegistry();
                }
                return 0;
            }
            break;

        case WM_DISPLAYCHANGE:
        case WM_SETTINGCHANGE:
            if (app && hwnd == app->hwnd && (msg == WM_DISPLAYCHANGE || wParam == SPI_SETWORKAREA)) {
                applyWindowScaleAndPosition(*app, false);
            }
            break;

        case WM_RBUTTONUP:
            if (app) {
                POINT pt = {};
                GetCursorPos(&pt);
                showContextMenu(*app, pt);
            }
            return 0;

        case WM_TRAYICON:
            if (app && (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP || lParam == WM_CONTEXTMENU)) {
                POINT pt = {};
                GetCursorPos(&pt);
                showContextMenu(*app, pt);
            }
            return 0;

        case WM_HOTKEY:
            if (app && wParam == HOTKEY_CLICKTHROUGH_ID) {
                app->settings.fullClickThrough = !app->settings.fullClickThrough;
                app->settings.saveToRegistry();
                if (app->settings.fullClickThrough) {
                    showTrayBalloon(*app,
                        L"KuroCat \u5DF2\u5F00\u542F\u5B8C\u5168\u9F20\u6807\u7A7F\u900F",
                        L"\u518D\u6B21\u6309 Ctrl+Alt+Shift+P \u6216\u6309\u4F4F Alt \u53F3\u952E\u5C0F\u732B\u5373\u53EF\u53D6\u6D88");
                } else {
                    showTrayBalloon(*app,
                        L"KuroCat \u5DF2\u53D6\u6D88\u5B8C\u5168\u9F20\u6807\u7A7F\u900F",
                        L"\u73B0\u5728\u53EF\u4EE5\u6B63\u5E38\u62D6\u52A8\u3001\u6478\u5934\u548C\u53F3\u952E\u6253\u5F00\u83DC\u5355\u4E86");
                }
            }
            return 0;

        case WM_WAKE_EXISTING:
            if (app) {
                // Re-launching KuroCat.exe unlocks click-through, restores visibility & opens settings menu!
                if (app->settings.fullClickThrough) {
                    app->settings.fullClickThrough = false;
                    app->settings.saveToRegistry();
                }
                if (!app->isVisible) {
                    setPetVisible(*app, true);
                }
                POINT pt = {};
                GetCursorPos(&pt);
                showContextMenu(*app, pt);
            }
            return 0;

        case WM_DESTROY:
            if (hwnd == (app ? app->hwnd : nullptr)) {
                KillTimer(hwnd, TIMER_RENDER);
                if (app) {
                    KillTimer(hwnd, TIMER_NOTIFICATIONS);
                    if (app->sessionEventsRegistered) WTSUnRegisterSessionNotification(hwnd);
                    app->notifications.stop();
                    Shell_NotifyIconW(NIM_DELETE, &app->nid);
                    if (app->menuOwnerHwnd) {
                        UnregisterHotKey(app->menuOwnerHwnd, HOTKEY_CLICKTHROUGH_ID);
                        DestroyWindow(app->menuOwnerHwnd);
                        app->menuOwnerHwnd = nullptr;
                    }
                    if (app->trayIcon) DestroyIcon(app->trayIcon);
                    app->destroyBuffer();
                }
                PostQuitMessage(0);
            }
            return 0;
    }
    if (g_msgTaskbarCreated != 0 && msg == g_msgTaskbarCreated && app && hwnd == app->menuOwnerHwnd) {
        registerTrayIcon(*app);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    // Single-instance guard: re-launching KuroCat.exe wakes the existing cat,
    // cancels full click-through, and pops up the menu so users NEVER get stuck!
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"Local\\KuroCatDesktopPetSingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND existing = FindWindowW(L"KuroCatTrayOwnerClass", nullptr)) {
            PostMessageW(existing, WM_WAKE_EXISTING, 0, 0);
        }
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    g_msgTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    Gdiplus::GdiplusStartupInput gdipInput;
    ULONG_PTR gdipToken = 0;
    Gdiplus::GdiplusStartup(&gdipToken, &gdipInput, nullptr);

    AppState app;
    g_app = &app;
    app.settings.loadFromRegistry();

    // Refresh auto-start exe path if user enabled auto-start and moved KuroCat.exe
    if (isAutoStartEnabled()) {
        setAutoStartEnabled(true);
    }

    app.input.onKeyDown = [&](PhysicalKey k) {
        if (app.isVisible) app.physics.triggerKeyDown(k, app.settings);
    };
    app.input.onMouseClick = [&](int btn, bool down) {
        if (app.isVisible) app.physics.triggerMouseClick(btn, down);
    };

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"KuroCatOverlayWindowClass";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    // Dedicated hidden tray/menu owner window (NO WS_EX_NOACTIVATE / WS_EX_TRANSPARENT)
    WNDCLASSEXW trayWc = {};
    trayWc.cbSize = sizeof(trayWc);
    trayWc.lpfnWndProc = wndProc;
    trayWc.hInstance = hInst;
    trayWc.lpszClassName = L"KuroCatTrayOwnerClass";
    RegisterClassExW(&trayWc);

    app.menuOwnerHwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        trayWc.lpszClassName,
        L"KuroCatTray",
        WS_POPUP,
        0, 0, 0, 0,
        nullptr, nullptr, hInst, nullptr
    );

    // Creation selects a monitor. The final size uses that window's DPI below.
    SIZE initialSize = petWindowSize(app.settings.scale, 96);
    POINT initialOrigin = {app.settings.posX, app.settings.posY};
    if (initialOrigin.x == -99999 || initialOrigin.y == -99999) initialOrigin = {0, 0};
    RECT initialWork = monitorWorkArea(MonitorFromPoint(initialOrigin, MONITOR_DEFAULTTONEAREST));
    initialOrigin = fitToWorkArea(initialOrigin, initialSize, initialWork);

    DWORD exStyle = WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT
                  | (app.settings.alwaysOnTop ? WS_EX_TOPMOST : 0);

    app.hwnd = CreateWindowExW(
        exStyle,
        wc.lpszClassName,
        kPetName,
        WS_POPUP,
        initialOrigin.x, initialOrigin.y, initialSize.cx, initialSize.cy,
        nullptr, nullptr, hInst, nullptr
    );
    if (!app.hwnd) {
        if (app.menuOwnerHwnd) DestroyWindow(app.menuOwnerHwnd);
        Gdiplus::GdiplusShutdown(gdipToken);
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    applyWindowScaleAndPosition(app, false);
    app.input.registerRawInput(app.hwnd);
    registerTrayIcon(app);

    // Global shortcut Ctrl+Alt+Shift+P to toggle full click-through anytime!
    if (app.menuOwnerHwnd) {
        RegisterHotKey(app.menuOwnerHwnd, HOTKEY_CLICKTHROUGH_ID, MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, 'P');
    }

    ShowWindow(app.hwnd, SW_SHOWNOACTIVATE);
    app.sessionEventsRegistered = WTSRegisterSessionNotification(app.hwnd, NOTIFY_FOR_THIS_SESSION) != FALSE;
    if (app.settings.notificationReactions) {
        // Saved opt-in can restore access silently, including after boot or unlock.
        SetTimer(app.hwnd, TIMER_NOTIFICATIONS, 1000, nullptr);
        refreshNotifications(app);
    }
    app.lastTickTime = nowSeconds();
    tickAndRender(app);
    SetTimer(app.hwnd, TIMER_RENDER, 16, nullptr);

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    Gdiplus::GdiplusShutdown(gdipToken);
    if (hMutex) CloseHandle(hMutex);
    return 0;
}
