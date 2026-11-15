#define UNICODE
#define _UNICODE
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>

#include "config.hpp"
#include "minecraft_launcher.hpp"
#include "server_pinger.hpp"

// Control IDs
#define IDC_TAB_MAIN       1001
#define IDC_TAB_LOGS       1002
#define IDC_TAB_SERVERS    1003
#define IDC_TAB_SETTINGS   1004

#define IDC_EDIT_NICKNAME  2001
#define IDC_COMBO_VERSION  2002
#define IDC_COMBO_LOADER   2003
#define IDC_COMBO_RAM      2004
#define IDC_BTN_PLAY       2005

#define IDC_EDIT_LOGS      3001
#define IDC_BTN_CLEAR_LOGS 3002

#define IDC_EDIT_SERVER    4001
#define IDC_BTN_PING       4002
#define IDC_LABEL_PING_RES 4003

#define IDC_COMBO_PRESET   5001
#define IDC_COMBO_THEME    5002
#define IDC_BTN_OPEN_DIR   5003

// Global State
static HINSTANCE g_hInstance = NULL;
static HWND g_hWndMain = NULL;
static LauncherConfig g_config;
static int g_currentTab = 0; // 0: Main, 1: Logs, 2: Servers, 3: Settings

// Controls
static HWND g_hEditNick = NULL;
static HWND g_hComboVer = NULL;
static HWND g_hComboLoader = NULL;
static HWND g_hComboRam = NULL;
static HWND g_hBtnPlay = NULL;
static HWND g_hLabelStatus = NULL;
static HWND g_hEditLogs = NULL;
static HWND g_hEditServer = NULL;
static HWND g_hLabelPingRes = NULL;

// Brushes and Fonts
static HBRUSH g_hBrushBg = NULL;
static HBRUSH g_hBrushSidebar = NULL;
static HBRUSH g_hBrushDock = NULL;
static HBRUSH g_hBrushEdit = NULL;
static HFONT g_hFontTitle = NULL;
static HFONT g_hFontText = NULL;
static HFONT g_hFontBold = NULL;
static HFONT g_hFontButton = NULL;
static HFONT g_hFontLogs = NULL;

// Game process tracking
static std::atomic<bool> g_isGameRunning(false);
static HANDLE g_hGameProcess = NULL;

// Theme Colors
static COLORREF g_colorAccent = RGB(157, 78, 221); // Void Purple
static COLORREF g_colorAccentLight = RGB(199, 125, 255);
static COLORREF g_colorBg = RGB(13, 15, 23);
static COLORREF g_colorSidebar = RGB(21, 24, 36);
static COLORREF g_colorDock = RGB(18, 20, 32);
static COLORREF g_colorText = RGB(240, 242, 248);
static COLORREF g_colorTextDim = RGB(140, 147, 168);

static void AppendLog(const std::wstring& line) {
    if (!g_hEditLogs) return;

    std::wstring formatted = line + L"\r\n";
    int len = GetWindowTextLengthW(g_hEditLogs);
    SendMessageW(g_hEditLogs, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageW(g_hEditLogs, EM_REPLACESEL, FALSE, (LPARAM)formatted.c_str());
    SendMessageW(g_hEditLogs, WM_VSCROLL, SB_BOTTOM, 0);
}

static void UpdateStatus(const std::wstring& status) {
    if (g_hLabelStatus) {
        SetWindowTextW(g_hLabelStatus, status.c_str());
    }
}

static void SwitchTab(int tabIndex) {
    g_currentTab = tabIndex;

    // Show / Hide Controls based on tab
    bool isLogs = (tabIndex == 1);
    bool isServers = (tabIndex == 2);

    ShowWindow(g_hEditLogs, isLogs ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(g_hWndMain, IDC_BTN_CLEAR_LOGS), isLogs ? SW_SHOW : SW_HIDE);

    ShowWindow(g_hEditServer, isServers ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(g_hWndMain, IDC_BTN_PING), isServers ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hLabelPingRes, isServers ? SW_SHOW : SW_HIDE);

    ShowWindow(GetDlgItem(g_hWndMain, IDC_COMBO_PRESET), (tabIndex == 3) ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(g_hWndMain, IDC_COMBO_THEME), (tabIndex == 3) ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(g_hWndMain, IDC_BTN_OPEN_DIR), (tabIndex == 3) ? SW_SHOW : SW_HIDE);

    InvalidateRect(g_hWndMain, NULL, TRUE);
}

static void OnPlayClicked() {
    if (g_isGameRunning) {
        // Stop Game
        UpdateStatus(L"Остановка игры...");
        MinecraftLauncher::TerminateRunningGame(g_hGameProcess);
        g_isGameRunning = false;
        SetWindowTextW(g_hBtnPlay, L"ИГРАТЬ");
        UpdateStatus(L"Готов к запуску");
        return;
    }

    // Save current dock settings
    WCHAR nickBuf[128];
    GetWindowTextW(g_hEditNick, nickBuf, 128);
    g_config.nickname = nickBuf;

    WCHAR verBuf[64];
    GetWindowTextW(g_hComboVer, verBuf, 64);
    g_config.version = verBuf;

    WCHAR loaderBuf[64];
    GetWindowTextW(g_hComboLoader, loaderBuf, 64);
    g_config.loader = loaderBuf;

    int ramSel = (int)SendMessageW(g_hComboRam, CB_GETCURSEL, 0, 0);
    int ramList[] = { 2048, 4096, 6144, 8192, 12288, 16384 };
    if (ramSel >= 0 && ramSel < 6) {
        g_config.memoryMb = ramList[ramSel];
    }

    SaveConfig(g_config);

    // Launch game in background thread
    g_isGameRunning = true;
    SetWindowTextW(g_hBtnPlay, L"ЗАКРЫТЬ ИГРУ");
    UpdateStatus(L"Подготовка запуска...");

    std::thread([=]() {
        LaunchProgressCallback cb;
        cb.onProgress = [](const std::wstring& status, int pct) {
            PostMessageW(g_hWndMain, WM_USER + 101, (WPARAM)pct, 0);
            UpdateStatus(status);
        };
        cb.onLog = [](const std::wstring& line, bool isErr) {
            AppendLog(line);
        };
        cb.onGameExited = [](int exitCode) {
            g_isGameRunning = false;
            PostMessageW(g_hWndMain, WM_USER + 102, (WPARAM)exitCode, 0);
        };

        bool ok = MinecraftLauncher::LaunchMinecraft(
            g_config.storagePath,
            g_config.version,
            g_config.loader,
            g_config.nickname,
            g_config.memoryMb,
            g_config.customJavaPath,
            g_config.jvmPreset,
            g_config.customJvmArgs,
            cb,
            &g_hGameProcess
        );

        if (!ok) {
            g_isGameRunning = false;
            PostMessageW(g_hWndMain, WM_USER + 102, (WPARAM)1, 0);
        }
    }).detach();
}

static void OnPingClicked() {
    WCHAR serverBuf[256];
    GetWindowTextW(g_hEditServer, serverBuf, 256);
    std::wstring serverW = serverBuf;
    if (serverW.empty()) return;

    SetWindowTextW(g_hLabelPingRes, L"Проверка соединения...");

    std::thread([serverW]() {
        std::string host(serverW.begin(), serverW.end());
        ServerStatus res = NativeServerPinger::Ping(host);

        WCHAR resBuf[512];
        if (res.online) {
            swprintf(resBuf, 512, L"Онлайн: %d/%d  |  Пинг: %d мс\nMOTD: %ls",
                res.playersOnline, res.playersMax, res.latencyMs, res.motd.c_str());
        } else {
            swprintf(resBuf, 512, L"Сервер недоступен или выключен");
        }

        SetWindowTextW(g_hLabelPingRes, resBuf);
    }).detach();
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_hWndMain = hwnd;

        // Fonts
        g_hFontTitle = CreateFontW(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        g_hFontText = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        g_hFontBold = CreateFontW(14, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        g_hFontButton = CreateFontW(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        g_hFontLogs = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");

        // Brushes
        g_hBrushBg = CreateSolidBrush(g_colorBg);
        g_hBrushSidebar = CreateSolidBrush(g_colorSidebar);
        g_hBrushDock = CreateSolidBrush(g_colorDock);
        g_hBrushEdit = CreateSolidBrush(RGB(28, 32, 48));

        // --- Bottom Dock Controls ---
        // 1. Nickname Edit Box (Right on the main dock!)
        g_hEditNick = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", g_config.nickname.c_str(),
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            200, 580, 160, 26,
            hwnd, (HMENU)IDC_EDIT_NICKNAME, g_hInstance, NULL
        );
        SendMessageW(g_hEditNick, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

        // 2. Version ComboBox (Right on the main dock!)
        g_hComboVer = CreateWindowW(
            L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            380, 580, 120, 250,
            hwnd, (HMENU)IDC_COMBO_VERSION, g_hInstance, NULL
        );
        SendMessageW(g_hComboVer, WM_SETFONT, (WPARAM)g_hFontText, TRUE);

        std::vector<std::wstring> vers = { L"1.21.1", L"1.20.4", L"1.20.1", L"1.19.4", L"1.18.2", L"1.16.5", L"1.12.2", L"1.7.10" };
        for (const auto& v : vers) {
            SendMessageW(g_hComboVer, CB_ADDSTRING, 0, (LPARAM)v.c_str());
        }
        int vIdx = (int)SendMessageW(g_hComboVer, CB_FINDSTRINGEXACT, -1, (LPARAM)g_config.version.c_str());
        SendMessageW(g_hComboVer, CB_SETCURSEL, vIdx >= 0 ? vIdx : 0, 0);

        // 3. Loader ComboBox
        g_hComboLoader = CreateWindowW(
            L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
            520, 580, 110, 150,
            hwnd, (HMENU)IDC_COMBO_LOADER, g_hInstance, NULL
        );
        SendMessageW(g_hComboLoader, WM_SETFONT, (WPARAM)g_hFontText, TRUE);
        SendMessageW(g_hComboLoader, CB_ADDSTRING, 0, (LPARAM)L"Vanilla");
        SendMessageW(g_hComboLoader, CB_ADDSTRING, 0, (LPARAM)L"Fabric");
        SendMessageW(g_hComboLoader, CB_ADDSTRING, 0, (LPARAM)L"Forge");
        SendMessageW(g_hComboLoader, CB_SETCURSEL, 0, 0);

        // 4. Memory ComboBox
        g_hComboRam = CreateWindowW(
            L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
            650, 580, 100, 150,
            hwnd, (HMENU)IDC_COMBO_RAM, g_hInstance, NULL
        );
        SendMessageW(g_hComboRam, WM_SETFONT, (WPARAM)g_hFontText, TRUE);
        SendMessageW(g_hComboRam, CB_ADDSTRING, 0, (LPARAM)L"2 ГБ RAM");
        SendMessageW(g_hComboRam, CB_ADDSTRING, 0, (LPARAM)L"4 ГБ RAM");
        SendMessageW(g_hComboRam, CB_ADDSTRING, 0, (LPARAM)L"6 ГБ RAM");
        SendMessageW(g_hComboRam, CB_ADDSTRING, 0, (LPARAM)L"8 ГБ RAM");
        SendMessageW(g_hComboRam, CB_ADDSTRING, 0, (LPARAM)L"12 ГБ RAM");
        SendMessageW(g_hComboRam, CB_ADDSTRING, 0, (LPARAM)L"16 ГБ RAM");
        SendMessageW(g_hComboRam, CB_SETCURSEL, 1, 0); // 4GB default

        // 5. PLAY BUTTON (Owner Drawn)
        g_hBtnPlay = CreateWindowW(
            L"BUTTON", L"ИГРАТЬ",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            780, 565, 180, 48,
            hwnd, (HMENU)IDC_BTN_PLAY, g_hInstance, NULL
        );

        // Status Label
        g_hLabelStatus = CreateWindowW(
            L"STATIC", L"Готов к запуску",
            WS_CHILD | WS_VISIBLE,
            200, 550, 400, 18,
            hwnd, NULL, g_hInstance, NULL
        );
        SendMessageW(g_hLabelStatus, WM_SETFONT, (WPARAM)g_hFontText, TRUE);

        // --- Tab 1: Logs Controls ---
        g_hEditLogs = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY,
            200, 70, 760, 460,
            hwnd, (HMENU)IDC_EDIT_LOGS, g_hInstance, NULL
        );
        SendMessageW(g_hEditLogs, WM_SETFONT, (WPARAM)g_hFontLogs, TRUE);

        HWND hBtnClear = CreateWindowW(
            L"BUTTON", L"Очистить логи",
            WS_CHILD | BS_PUSHBUTTON,
            840, 30, 120, 28,
            hwnd, (HMENU)IDC_BTN_CLEAR_LOGS, g_hInstance, NULL
        );
        SendMessageW(hBtnClear, WM_SETFONT, (WPARAM)g_hFontText, TRUE);

        // --- Tab 2: Server Controls ---
        g_hEditServer = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", L"mc.hypixel.net",
            WS_CHILD | ES_AUTOHSCROLL,
            200, 70, 240, 28,
            hwnd, (HMENU)IDC_EDIT_SERVER, g_hInstance, NULL
        );
        SendMessageW(g_hEditServer, WM_SETFONT, (WPARAM)g_hFontText, TRUE);

        HWND hBtnPing = CreateWindowW(
            L"BUTTON", L"Проверить пинг",
            WS_CHILD | BS_PUSHBUTTON,
            460, 70, 130, 28,
            hwnd, (HMENU)IDC_BTN_PING, g_hInstance, NULL
        );
        SendMessageW(hBtnPing, WM_SETFONT, (WPARAM)g_hFontText, TRUE);

        g_hLabelPingRes = CreateWindowW(
            L"STATIC", L"Введите адрес сервера и нажмите «Проверить пинг»",
            WS_CHILD,
            200, 120, 500, 80,
            hwnd, (HMENU)IDC_LABEL_PING_RES, g_hInstance, NULL
        );
        SendMessageW(g_hLabelPingRes, WM_SETFONT, (WPARAM)g_hFontText, TRUE);

        // --- Tab 3: Settings Controls ---
        HWND hPreset = CreateWindowW(
            L"COMBOBOX", L"",
            WS_CHILD | CBS_DROPDOWNLIST,
            200, 70, 300, 150,
            hwnd, (HMENU)IDC_COMBO_PRESET, g_hInstance, NULL
        );
        SendMessageW(hPreset, WM_SETFONT, (WPARAM)g_hFontText, TRUE);
        SendMessageW(hPreset, CB_ADDSTRING, 0, (LPARAM)L"Флаги Aikar (G1GC без микрофризов)");
        SendMessageW(hPreset, CB_ADDSTRING, 0, (LPARAM)L"Low RAM / Potato PC (Экономия ОЗУ)");
        SendMessageW(hPreset, CB_ADDSTRING, 0, (LPARAM)L"Стандартные флаги HotSpot");
        SendMessageW(hPreset, CB_SETCURSEL, 0, 0);

        HWND hTheme = CreateWindowW(
            L"COMBOBOX", L"",
            WS_CHILD | CBS_DROPDOWNLIST,
            200, 120, 300, 150,
            hwnd, (HMENU)IDC_COMBO_THEME, g_hInstance, NULL
        );
        SendMessageW(hTheme, WM_SETFONT, (WPARAM)g_hFontText, TRUE);
        SendMessageW(hTheme, CB_ADDSTRING, 0, (LPARAM)L"Void Purple (Фиолетовая бездна)");
        SendMessageW(hTheme, CB_ADDSTRING, 0, (LPARAM)L"Crimson Red (Багровый рубин)");
        SendMessageW(hTheme, CB_ADDSTRING, 0, (LPARAM)L"Cyberpunk Cyan (Киберпанк)");
        SendMessageW(hTheme, CB_SETCURSEL, 0, 0);

        HWND hOpenDir = CreateWindowW(
            L"BUTTON", L"Открыть папку хранилища лаунчера",
            WS_CHILD | BS_PUSHBUTTON,
            200, 170, 300, 32,
            hwnd, (HMENU)IDC_BTN_OPEN_DIR, g_hInstance, NULL
        );
        SendMessageW(hOpenDir, WM_SETFONT, (WPARAM)g_hFontText, TRUE);

        SwitchTab(0);
        return 0;
    }

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        switch (wmId) {
        case IDC_BTN_PLAY:
            OnPlayClicked();
            break;
        case IDC_BTN_CLEAR_LOGS:
            SetWindowTextW(g_hEditLogs, L"");
            break;
        case IDC_BTN_PING:
            OnPingClicked();
            break;
        case IDC_BTN_OPEN_DIR:
            ShellExecuteW(NULL, L"open", g_config.storagePath.c_str(), NULL, NULL, SW_SHOWNORMAL);
            break;
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);

        // Sidebar Tab Clicking (x < 180)
        if (x < 180 && y >= 80 && y < 320) {
            int clickedTab = (y - 80) / 45;
            if (clickedTab >= 0 && clickedTab < 4) {
                SwitchTab(clickedTab);
            }
        }
        return 0;
    }

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
        if (dis->CtlID == IDC_BTN_PLAY) {
            // Draw custom modern glowing Play Button
            HDC hdc = dis->hDC;
            RECT rc = dis->rcItem;

            COLORREF btnColor = g_isGameRunning ? RGB(225, 29, 72) : g_colorAccent;
            HBRUSH hBtnBrush = CreateSolidBrush(btnColor);
            FillRect(hdc, &rc, hBtnBrush);
            DeleteObject(hBtnBrush);

            // Button Border
            HPEN hPen = CreatePen(PS_SOLID, 2, g_colorAccentLight);
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 12, 12);
            SelectObject(hdc, hOldPen);
            SelectObject(hdc, hOldBrush);
            DeleteObject(hPen);

            // Button Text
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            SelectObject(hdc, g_hFontButton);

            WCHAR btnText[64];
            GetWindowTextW(g_hBtnPlay, btnText, 64);
            DrawTextW(hdc, btnText, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }
        break;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        HWND hCtl = (HWND)lParam;
        SetTextColor(hdc, g_colorText);
        SetBkColor(hdc, g_colorDock);
        return (INT_PTR)g_hBrushDock;
    }

    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkColor(hdc, RGB(28, 32, 48));
        return (INT_PTR)g_hBrushEdit;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT clientRc;
        GetClientRect(hwnd, &clientRc);

        // 1. Background
        FillRect(hdc, &clientRc, g_hBrushBg);

        // 2. Sidebar (Left 0..180)
        RECT sidebarRc = { 0, 0, 180, clientRc.bottom };
        FillRect(hdc, &sidebarRc, g_hBrushSidebar);

        // Brand Title
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        SelectObject(hdc, g_hFontTitle);
        TextOutW(hdc, 20, 20, L"Aether", 6);

        SetTextColor(hdc, g_colorAccentLight);
        SelectObject(hdc, g_hFontBold);
        TextOutW(hdc, 22, 46, L"LAUNCHER v1.0.38", 16);

        // Sidebar Tabs
        const WCHAR* tabNames[] = {
            L"🎮  Главная",
            L"📜  Логи игры",
            L"📡  Серверы",
            L"⚙️  Настройки"
        };

        SelectObject(hdc, g_hFontBold);
        for (int i = 0; i < 4; i++) {
            RECT tabRc = { 10, 80 + i * 45, 170, 80 + (i + 1) * 45 - 5 };
            if (i == g_currentTab) {
                HBRUSH hActiveTab = CreateSolidBrush(RGB(35, 40, 60));
                FillRect(hdc, &tabRc, hActiveTab);
                DeleteObject(hActiveTab);
                SetTextColor(hdc, g_colorAccentLight);
            } else {
                SetTextColor(hdc, g_colorTextDim);
            }
            DrawTextW(hdc, tabNames[i], -1, &tabRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }

        // 3. Tab Contents Header
        SetTextColor(hdc, RGB(255, 255, 255));
        SelectObject(hdc, g_hFontTitle);
        if (g_currentTab == 0) {
            TextOutW(hdc, 200, 20, L"Minecraft Client", 16);
            SetTextColor(hdc, g_colorTextDim);
            SelectObject(hdc, g_hFontText);
            TextOutW(hdc, 202, 50, L"Выберите версию, никнейм и нажмите «ИГРАТЬ»", 44);

            // Container Info Box
            RECT infoRc = { 200, 80, 750, 220 };
            HBRUSH hCard = CreateSolidBrush(RGB(22, 26, 38));
            FillRect(hdc, &infoRc, hCard);
            DeleteObject(hCard);

            SetTextColor(hdc, g_colorText);
            SelectObject(hdc, g_hFontBold);
            TextOutW(hdc, 220, 100, L"Готов к быстрому запуску игры", 29);

            SetTextColor(hdc, g_colorTextDim);
            SelectObject(hdc, g_hFontText);
            TextOutW(hdc, 220, 130, L"• Прямая загрузка официальных файлов Minecraft с серверов Mojang", 64);
            TextOutW(hdc, 220, 155, L"• Настройка никнейма прямо в главном окне без захода в настройки", 63);
            TextOutW(hdc, 220, 180, L"• 0% нагрузки на процессор во время игры", 40);
        } else if (g_currentTab == 1) {
            TextOutW(hdc, 200, 20, L"Логи и диагностика крашей", 25);
        } else if (g_currentTab == 2) {
            TextOutW(hdc, 200, 20, L"Мониторинг серверов", 19);
        } else if (g_currentTab == 3) {
            TextOutW(hdc, 200, 20, L"Настройки лаунчера", 18);
        }

        // 4. Bottom Dock (Bottom 100px)
        RECT dockRc = { 0, 530, clientRc.right, clientRc.bottom };
        FillRect(hdc, &dockRc, g_hBrushDock);

        // Top Border of Dock
        HPEN hDockBorder = CreatePen(PS_SOLID, 1, RGB(42, 46, 66));
        HPEN hOldPen = (HPEN)SelectObject(hdc, hDockBorder);
        MoveToEx(hdc, 0, 530, NULL);
        LineTo(hdc, clientRc.right, 530);
        SelectObject(hdc, hOldPen);
        DeleteObject(hDockBorder);

        // Labels in Dock
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, g_colorAccentLight);
        SelectObject(hdc, g_hFontBold);
        TextOutW(hdc, 200, 558, L"Никнейм:", 8);
        TextOutW(hdc, 380, 558, L"Версия:", 7);
        TextOutW(hdc, 520, 558, L"Загрузчик:", 10);
        TextOutW(hdc, 650, 558, L"Память:", 7);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_USER + 101: {
        // Progress update
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_USER + 102: {
        // Game exited
        int exitCode = (int)wParam;
        g_isGameRunning = false;
        SetWindowTextW(g_hBtnPlay, L"ИГРАТЬ");

        if (exitCode != 0) {
            UpdateStatus(L"Игра завершилась с ошибкой!");
            SwitchTab(1); // Switch to logs tab on error!
        } else {
            UpdateStatus(L"Игра завершена");
        }
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_DESTROY: {
        // Save config on close
        if (g_hEditNick) {
            WCHAR nickBuf[128];
            GetWindowTextW(g_hEditNick, nickBuf, 128);
            g_config.nickname = nickBuf;
            SaveConfig(g_config);
        }

        DeleteObject(g_hBrushBg);
        DeleteObject(g_hBrushSidebar);
        DeleteObject(g_hBrushDock);
        DeleteObject(g_hBrushEdit);
        DeleteObject(g_hFontTitle);
        DeleteObject(g_hFontText);
        DeleteObject(g_hFontBold);
        DeleteObject(g_hFontButton);
        DeleteObject(g_hFontLogs);

        PostQuitMessage(0);
        return 0;
    }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_hInstance = hInstance;
    g_config = LoadConfig();

    // Single Instance Mutex
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"AetherLauncherCppSingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(L"AetherLauncherCppClass", NULL);
        if (existing) {
            SetForegroundWindow(existing);
            ShowWindow(existing, SW_RESTORE);
        }
        return 0;
    }

    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS };
    InitCommonControlsEx(&icex);

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"AetherLauncherCppClass";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        0,
        L"AetherLauncherCppClass",
        L"Aether Launcher — Minecraft Client",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1000, 660,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) return 1;

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CloseHandle(hMutex);
    return (int)msg.wParam;
}
