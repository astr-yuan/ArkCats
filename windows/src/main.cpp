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
#include <shobjidl.h>
#include <objidl.h>
#include <gdiplus.h>
#include <string>
#include <string_view>
#include <cmath>
#include <algorithm>
#include <iterator>
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

constexpr wchar_t kAppName[] = L"ArkCats";
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
    CMD_MIGRATE_AUTOSTART,
    CMD_TOGGLE_BADGES,
    CMD_TOGGLE_FOOT,
    CMD_TOGGLE_PET,
    CMD_COAT_BLACK,
    CMD_COAT_SIAMESE,
    CMD_COAT_SHORTHAIR,
    CMD_COAT_ORANGE_WHITE,
    CMD_COAT_SAPPHIRE_TWIN_TAIL,
    CMD_COAT_WANG,
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
    CMD_DEMO_GO_STONE,
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
    bool clickHotkeyRegistered = false;

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

struct SkinMenuEntry {
    UINT command;
    CatCoatTheme coat;
    const wchar_t* label;
};

constexpr SkinMenuEntry skins[] = {
    {CMD_COAT_SAPPHIRE_TWIN_TAIL, CatCoatTheme::SapphireTwinTail, L"Miss. Christine"},
    {CMD_COAT_WANG, CatCoatTheme::Wang, L"望的丑猫"},
    {CMD_COAT_BLACK, CatCoatTheme::ClassicBlack, L"经典小黑猫（KuroCat）"},
    {CMD_COAT_SIAMESE, CatCoatTheme::Siamese, L"挖煤暹罗猫（KuroCat）"},
    {CMD_COAT_SHORTHAIR, CatCoatTheme::Shorthair, L"美短（银虎斑加白）（KuroCat）"},
    {CMD_COAT_ORANGE_WHITE, CatCoatTheme::OrangeWhite, L"橘白猫（KuroCat）"}
};

HICON createCatTrayIcon(CatCoatTheme coat);

constexpr wchar_t runKeyPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

std::wstring currentExePath() {
    std::wstring path(32768, L'\0');
    DWORD n = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!n || n >= path.size()) return {};
    path.resize(n);
    return path;
}

std::wstring quoted(const std::wstring& path) { return L"\"" + path + L"\""; }

LONG readRunCommand(HKEY key, const wchar_t* name, std::wstring& result) {
    DWORD bytes = 0;
    LONG status = RegGetValueW(key, nullptr, name, RRF_RT_REG_SZ, nullptr, nullptr, &bytes);
    if (status != ERROR_SUCCESS) return status;
    if (bytes < sizeof(wchar_t) || bytes % sizeof(wchar_t) || bytes > 65536) return ERROR_INVALID_DATA;
    std::wstring value(bytes / sizeof(wchar_t), L'\0');
    status = RegGetValueW(key, nullptr, name, RRF_RT_REG_SZ, nullptr, value.data(), &bytes);
    if (status != ERROR_SUCCESS) return status;
    value.resize(bytes / sizeof(wchar_t));
    if (value.empty() || value.back() != L'\0') return ERROR_INVALID_DATA;
    value.pop_back();
    if (value.find(L'\0') != std::wstring::npos) return ERROR_INVALID_DATA;
    result = std::move(value);
    return ERROR_SUCCESS;
}

bool commandMatches(const std::wstring& actual, const std::wstring& path) {
    return !path.empty() && _wcsicmp(actual.c_str(), quoted(path).c_str()) == 0;
}

bool isAutoStartEnabled() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, runKeyPath, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return false;
    std::wstring command;
    bool enabled = readRunCommand(key, kAppName, command) == ERROR_SUCCESS && commandMatches(command, currentExePath());
    RegCloseKey(key);
    return enabled;
}

bool writeRunCommand(HKEY key, const std::wstring& path) {
    if (path.empty()) return false;
    std::wstring command = quoted(path);
    return RegSetValueExW(key, kAppName, 0, REG_SZ, reinterpret_cast<const BYTE*>(command.c_str()),
                         static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
}

bool setAutoStartEnabled(bool enable) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, runKeyPath, 0, nullptr, 0,
                        KEY_QUERY_VALUE | KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) return false;
    bool ok = false;
    std::wstring path = currentExePath(), old;
    if (enable) ok = writeRunCommand(key, path);
    else if (readRunCommand(key, kAppName, old) == ERROR_SUCCESS && commandMatches(old, path)) {
        ok = RegDeleteValueW(key, kAppName) == ERROR_SUCCESS;
    }
    RegCloseKey(key);
    return ok;
}

bool hasLegacyManifest(const std::wstring& path, std::string_view expectedDescription) {
    HMODULE module = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    if (!module) return false;
    HRSRC res = FindResourceW(module, MAKEINTRESOURCEW(1), MAKEINTRESOURCEW(24));
    HGLOBAL data = res ? LoadResource(module, res) : nullptr;
    const char* xml = data ? static_cast<const char*>(LockResource(data)) : nullptr;
    bool matched = xml && std::string_view(xml, SizeofResource(module, res)).find(expectedDescription) != std::string_view::npos;
    FreeLibrary(module);
    return matched;
}

// Only the named legacy entries written by our releases, pointing at a verified
// sibling executable, are eligible. The menu lets the user explicitly migrate them.
bool migrateLegacyAutostart(bool apply) {
    HKEY key = nullptr;
    REGSAM rights = KEY_QUERY_VALUE | (apply ? KEY_SET_VALUE : 0);
    if (RegOpenKeyExW(HKEY_CURRENT_USER, runKeyPath, 0, rights, &key) != ERROR_SUCCESS) return false;
    std::wstring path = currentExePath(), existing;
    auto slash = path.find_last_of(L"\\/");
    LONG status = readRunCommand(key, kAppName, existing);
    if (slash == std::wstring::npos || (status != ERROR_FILE_NOT_FOUND &&
            (status != ERROR_SUCCESS || !commandMatches(existing, path)))) {
        RegCloseKey(key);
        return false;
    }
    struct LegacyEntry { const wchar_t* name; const char* description; };
    const LegacyEntry legacy[] = {
        {L"KuroCat", u8"<description>Miss. Christine Desktop Pet</description>"},
        {L"WangCat", u8"<description>望的丑猫</description>"}
    };
    bool eligible = false, failed = false;
    for (const auto& entry : legacy) {
        std::wstring oldCommand;
        std::wstring oldPath = path.substr(0, slash + 1) + entry.name + L".exe";
        if (readRunCommand(key, entry.name, oldCommand) != ERROR_SUCCESS || !commandMatches(oldCommand, oldPath) ||
            !hasLegacyManifest(oldPath, entry.description)) continue;
        eligible = true;
        if (!apply) break;
        if (status == ERROR_FILE_NOT_FOUND) {
            if (!writeRunCommand(key, path)) { eligible = false; break; }
            status = ERROR_SUCCESS;
        }
        // Recheck the value before removing it in case another installer changed it.
        if (readRunCommand(key, entry.name, oldCommand) == ERROR_SUCCESS && commandMatches(oldCommand, oldPath))
            failed |= RegDeleteValueW(key, entry.name) != ERROR_SUCCESS;
    }
    RegCloseKey(key);
    return eligible && !failed;
}

void showTrayBalloon(AppState& app, const wchar_t* title, const wchar_t* body) {
    if (app.settings.notificationReactions) return;
    NOTIFYICONDATAW b = app.nid;
    b.uFlags = NIF_INFO;
    b.dwInfoFlags = NIIF_INFO | NIIF_NOSOUND;
    wcsncpy_s(b.szInfoTitle, title, _TRUNCATE);
    wcsncpy_s(b.szInfo, body, _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &b);
}

void toggleFullClickThrough(AppState& app) {
    app.settings.fullClickThrough = !app.settings.fullClickThrough;
    app.settings.saveToRegistry();
    if (app.settings.fullClickThrough) {
        const wchar_t* body = app.clickHotkeyRegistered
            ? L"按 Ctrl+Alt+Shift+P 或从 ArkCats 托盘菜单取消。"
            : L"从 ArkCats 托盘菜单取消，或再次运行 ArkCats.exe。";
        showTrayBalloon(app, L"ArkCats 已开启鼠标穿透", body);
    } else {
        showTrayBalloon(app, L"ArkCats 已关闭鼠标穿透", L"现在可以拖动、摸头或右键打开菜单。");
    }
}

void registerTrayIcon(AppState& app, bool update = false) {
    HICON previous = app.trayIcon;
    if (update || !app.trayIcon) {
        HICON next = createCatTrayIcon(app.settings.coatTheme);
        if (next) app.trayIcon = next;
    }
    app.nid = {};
    app.nid.cbSize = sizeof(app.nid);
    app.nid.hWnd = app.menuOwnerHwnd ? app.menuOwnerHwnd : app.hwnd;
    app.nid.uID = 1;
    app.nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    app.nid.uCallbackMessage = WM_TRAYICON;
    app.nid.hIcon = app.trayIcon;
    std::wstring title = kAppName;
    for (const auto& skin : skins) {
        if (skin.coat == app.settings.coatTheme) { title += L" · "; title += skin.label; break; }
    }
    SetWindowTextW(app.hwnd, title.c_str());
    SendMessageW(app.hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(app.trayIcon));
    SendMessageW(app.hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(app.trayIcon));
    wcsncpy_s(app.nid.szTip, title.c_str(), _TRUNCATE);
    Shell_NotifyIconW(update ? NIM_MODIFY : NIM_ADD, &app.nid);
    if (previous && previous != app.trayIcon) DestroyIcon(previous);
}

HICON createCatTrayIcon(CatCoatTheme coat) {
    using namespace Gdiplus;
    constexpr int sz = 32;
    Bitmap bmp(sz, sz, PixelFormat32bppPARGB);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.Clear(Color(0, 0, 0, 0));

    if (coat == CatCoatTheme::Wang) {
        SolidBrush ink(Color(255, 38, 37, 44));
        SolidBrush slate(Color(255, 69, 69, 79));
        SolidBrush porcelain(Color(255, 248, 246, 237));
        SolidBrush gold(Color(255, 217, 178, 111));
        PointF leftEar[] = {{2, 14}, {5, 2}, {14, 8}};
        PointF rightEar[] = {{18, 8}, {26, 2}, {30, 14}};
        g.FillPolygon(&slate, leftEar, 3);
        g.FillPolygon(&slate, rightEar, 3);
        GraphicsPath mask;
        mask.AddBezier(16.f, 6.f, 3.f, 5.f, 1.f, 14.f, 3.5f, 19.f);
        mask.AddBezier(3.5f, 19.f, 5.f, 25.f, 12.f, 26.f, 16.f, 28.f);
        mask.AddBezier(16.f, 28.f, 22.f, 26.f, 27.f, 25.f, 28.5f, 19.f);
        mask.AddBezier(28.5f, 19.f, 31.f, 13.f, 27.f, 5.f, 16.f, 6.f);
        mask.CloseFigure();
        g.FillPath(&slate, &mask);
        GraphicsState face = g.Save();
        g.SetClip(&mask);
        g.FillRectangle(&porcelain, 16.f, 5.f, 15.f, 24.f);
        g.Restore(face);
        Pen outline(Color(255, 34, 33, 39), 1.3f);
        g.DrawPath(&outline, &mask);
        g.DrawLine(&outline, 16.f, 6.5f, 16.f, 26.f);

        g.FillEllipse(&porcelain, 5.4f, 12.6f, 8.f, 8.5f);
        g.FillEllipse(&ink, 9.0f, 16.f, 1.7f, 1.7f);
        g.FillEllipse(&gold, 18.4f, 12.6f, 8.2f, 8.5f);
        g.FillEllipse(&ink, 19.2f, 13.3f, 6.7f, 7.f);
        g.FillEllipse(&porcelain, 22.3f, 15.1f, 1.8f, 1.8f);
        PointF pendant[] = {{16, 24}, {17.5f, 26.5f}, {20, 27.5f}, {17.5f, 29},
                            {16, 31}, {14.5f, 29}, {12, 27.5f}, {14.5f, 26.5f}};
        g.FillPolygon(&gold, pendant, 8);
    } else {
        SolidBrush black(Color(255, 22, 22, 26));
        PointF lEar[3] = { {4, 15}, {6, 3}, {14, 11} };
        PointF rEar[3] = { {28, 15}, {26, 3}, {18, 11} };
        g.FillPolygon(&black, lEar, 3);
        g.FillPolygon(&black, rEar, 3);
        Color furColor(255, 22, 22, 26);
        if (coat == CatCoatTheme::Siamese) furColor = Color(255, 122, 90, 76);
        if (coat == CatCoatTheme::Shorthair) furColor = Color(255, 167, 170, 169);
        if (coat == CatCoatTheme::OrangeWhite) furColor = Color(255, 223, 155, 89);
        SolidBrush fur(furColor);
        g.FillEllipse(&fur, 4.0f, 8.0f, 24.0f, 21.0f);
        if (coat == CatCoatTheme::OrangeWhite || coat == CatCoatTheme::Shorthair) {
            SolidBrush milk(Color(255, 247, 243, 232));
            g.FillEllipse(&milk, 11.f, 20.f, 10.f, 6.f);
        }

        Color iris = coat == CatCoatTheme::ClassicBlack ? Color(255, 202, 221, 103)
                   : (coat == CatCoatTheme::Shorthair || coat == CatCoatTheme::OrangeWhite)
                        ? Color(255, 222, 179, 119) : Color(255, 94, 199, 250);
        SolidBrush blue(iris);
        g.FillEllipse(&blue, 8.5f, 14.5f, 6.0f, 5.2f);
        g.FillEllipse(&blue, 17.5f, 14.5f, 6.0f, 5.2f);
        g.FillEllipse(&black, 10.7f, 15.2f, 1.8f, 3.8f);
        g.FillEllipse(&black, 19.7f, 15.2f, 1.8f, 3.8f);

        if (coat == CatCoatTheme::SapphireTwinTail) {
            SolidBrush red(Color(255, 225, 43, 61));
            PointF bowLeft[] = {{8, 22}, {16, 25}, {8, 29}};
            PointF bowRight[] = {{24, 22}, {24, 29}, {16, 25}};
            g.FillPolygon(&red, bowLeft, 3);
            g.FillPolygon(&red, bowRight, 3);
            g.FillEllipse(&blue, 14.0f, 23.0f, 4.0f, 5.0f);
        }
    }

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
    const wchar_t* clickLabel = app.clickHotkeyRegistered
        ? L"完全鼠标穿透 (Ctrl+Alt+Shift+P / 按住 Alt 可右键)"
        : L"完全鼠标穿透 (按住 Alt 可右键)";
    AppendMenuW(menu, MF_STRING | (app.settings.fullClickThrough ? MF_CHECKED : 0), CMD_TOGGLE_CLICKTHROUGH, clickLabel);
    AppendMenuW(menu, MF_STRING | (app.settings.alwaysOnTop ? MF_CHECKED : 0), CMD_TOGGLE_TOPMOST, L"\u7A97\u53E3\u7F6E\u9876\u663E\u793A");
    if (migrateLegacyAutostart(false))
        AppendMenuW(menu, MF_STRING, CMD_MIGRATE_AUTOSTART, L"将此目录旧版的开机启动迁移到 ArkCats");
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

    HMENU coatMenu = CreatePopupMenu();
    int selected = 0;
    for (int i = 0; i < static_cast<int>(std::size(skins)); ++i) {
        const auto& skin = skins[i];
        AppendMenuW(coatMenu, MF_STRING, skin.command, skin.label);
        if (app.settings.coatTheme == skin.coat) selected = i;
    }
    CheckMenuRadioItem(coatMenu, 0, static_cast<UINT>(std::size(skins) - 1), selected, MF_BYPOSITION);
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(coatMenu), L"皮肤选择");

    // Keyboard Theme submenu
    HMENU kbMenu = CreatePopupMenu();
    AppendMenuW(kbMenu, MF_STRING | (app.settings.keyboardTheme == KeyboardTheme::ClassicMilk ? MF_CHECKED : 0), CMD_KB_MILK, L"\u7ECF\u5178\u5976\u767D\u6696\u9EC4 (\u539F\u7248)");
    AppendMenuW(kbMenu, MF_STRING | (app.settings.keyboardTheme == KeyboardTheme::MidnightHacker ? MF_CHECKED : 0), CMD_KB_HACKER, L"\u6697\u591C\u6781\u5BA2\u9ED1");
    AppendMenuW(kbMenu, MF_STRING | (app.settings.keyboardTheme == KeyboardTheme::RetroCream ? MF_CHECKED : 0), CMD_KB_RETRO, L"\u590D\u53E4\u5976\u6CB9\u7070");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)kbMenu, L"\u952E\u76D8\u914D\u8272");

    if (app.settings.coatTheme != CatCoatTheme::Wang) {
        HMENU eyeMenu = CreatePopupMenu();
        AppendMenuW(eyeMenu, MF_STRING | (app.settings.eyeTheme == EyeColorTheme::NeonChartreuse ? MF_CHECKED : 0), CMD_EYE_LIME, L"\u7ECF\u5178\u8367\u5149\u9EC4\u7EFF (\u539F\u7248)");
        AppendMenuW(eyeMenu, MF_STRING | (app.settings.eyeTheme == EyeColorTheme::SiameseBlue ? MF_CHECKED : 0), CMD_EYE_BLUE, L"\u66B9\u7F57\u5B9D\u77F3\u84DD");
        AppendMenuW(eyeMenu, MF_STRING | (app.settings.eyeTheme == EyeColorTheme::WarmAmber ? MF_CHECKED : 0), CMD_EYE_AMBER, L"\u7425\u73C0\u91D1\u77B3");
        AppendMenuW(eyeMenu, MF_STRING | (app.settings.eyeTheme == EyeColorTheme::IceCyan ? MF_CHECKED : 0), CMD_EYE_CYAN, L"\u51B0\u6676\u6D45\u84DD");
        AppendMenuW(eyeMenu, MF_STRING | (app.settings.eyeTheme == EyeColorTheme::SoftAmber ? MF_CHECKED : 0), CMD_EYE_SOFT_AMBER, L"柔和琥珀");
        AppendMenuW(eyeMenu, MF_STRING | (app.settings.eyeTheme == EyeColorTheme::ReferenceHazel ? MF_CHECKED : 0), CMD_EYE_REFERENCE_HAZEL, L"淡金青绿");
        AppendMenuW(menu, MF_POPUP, (UINT_PTR)eyeMenu, L"\u77B3\u8272\u5207\u6362");
    }

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
    if (app.settings.coatTheme == CatCoatTheme::Wang)
        AppendMenuW(menu, MF_STRING, CMD_DEMO_GO_STONE, L"演示落子");
    AppendMenuW(menu, MF_STRING, CMD_DEMO_BURST, L"\u6F14\u793A\u6253\u5B57\u52A8\u753B");
    AppendMenuW(menu, MF_STRING, CMD_RESET_POS, L"\u91CD\u7F6E\u4F4D\u7F6E\u5230\u53F3\u4E0B\u89D2");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, CMD_QUIT, L"\u9000\u51FA ArkCats");

    HWND menuOwner = app.menuOwnerHwnd ? app.menuOwnerHwnd : app.hwnd;
    SetForegroundWindow(menuOwner);
    UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, screenPt.x, screenPt.y, 0, menuOwner, nullptr);
    PostMessageW(menuOwner, WM_NULL, 0, 0);
    DestroyMenu(menu);

    for (const auto& skin : skins) {
        if (cmd == skin.command) {
            if (skin.coat != app.settings.coatTheme) {
                app.settings.selectCoat(skin.coat);
                app.physics.setCoat(app.settings.coatTheme);
                app.settings.saveToRegistry();
                registerTrayIcon(app, true);
            }
            return;
        }
    }

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
            toggleFullClickThrough(app);
            break;
        case CMD_TOGGLE_TOPMOST:
            app.settings.alwaysOnTop = !app.settings.alwaysOnTop;
            applyWindowScaleAndPosition(app, false);
            break;
        case CMD_MIGRATE_AUTOSTART:
            if (!migrateLegacyAutostart(true))
                MessageBoxW(menuOwner, L"未能完成开机启动迁移，请重新打开菜单检查。", kAppName, MB_OK | MB_ICONINFORMATION);
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
        case CMD_DEMO_GO_STONE:
            if (app.settings.coatTheme == CatCoatTheme::Wang) {
                if (!app.isVisible) setPetVisible(app, true);
                app.physics.placeGoStone();
            }
            break;
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
            if (app && app->clickHotkeyRegistered && hwnd == app->menuOwnerHwnd && wParam == HOTKEY_CLICKTHROUGH_ID) {
                toggleFullClickThrough(*app);
            }
            return 0;

        case WM_WAKE_EXISTING:
            if (app) {
                // Re-launching ArkCats.exe unlocks click-through, restores visibility & opens settings menu!
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
    SetCurrentProcessExplicitAppUserModelID(L"ArkCats.DesktopPet");
    // Single-instance guard: re-launching ArkCats.exe wakes the existing cat,
    // cancels full click-through, and pops up the menu so users NEVER get stuck!
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"Local\\ArkCatsDesktopPetSingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND existing = FindWindowW(L"ArkCatsTrayOwnerClass", nullptr)) {
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
    app.physics.setCoat(app.settings.coatTheme);

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
    wc.lpszClassName = L"ArkCatsOverlayWindowClass";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    // Dedicated hidden tray/menu owner window (NO WS_EX_NOACTIVATE / WS_EX_TRANSPARENT)
    WNDCLASSEXW trayWc = {};
    trayWc.cbSize = sizeof(trayWc);
    trayWc.lpfnWndProc = wndProc;
    trayWc.hInstance = hInst;
    trayWc.lpszClassName = L"ArkCatsTrayOwnerClass";
    RegisterClassExW(&trayWc);

    app.menuOwnerHwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        trayWc.lpszClassName,
        L"ArkCatsTray",
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
        kAppName,
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
        app.clickHotkeyRegistered = RegisterHotKey(app.menuOwnerHwnd, HOTKEY_CLICKTHROUGH_ID, MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, 'P') != FALSE;
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
