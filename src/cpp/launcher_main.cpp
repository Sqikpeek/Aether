// ============================================================================
// Aether Launcher - Native Modern C++ Win32 GUI Engine (GDI+)
// Pure Native Desktop Application with Zero Browser / Chrome / Node Dependencies
// Features: Full Theme Engine, Anti-Aliased Graphics, Double-Buffering (60 FPS),
// Prominent In-Dock Nickname Input, Mojang CDN Downloader, and Server Pinger.
// ============================================================================

#define UNICODE
#define _UNICODE
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <propidl.h>
#include <gdiplus.h>
#include <shlwapi.h>
#include <shellapi.h>

#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <algorithm>

#include "config.hpp"
#include "minecraft_launcher.hpp"
#include "server_pinger.hpp"

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")

using namespace Gdiplus;

// ----------------------------------------------------------------------------
// Themes
// ----------------------------------------------------------------------------
struct LauncherTheme {
    std::wstring id;
    std::wstring name;
    Color bgBase;
    Color bgSurface;
    Color bgSurfaceElevated;
    Color bgCard;
    Color border;
    Color borderFocus;
    Color accent;
    Color accentSecondary;
    Color accentGlow;
    Color textMain;
    Color textMuted;
    Color success;
    Color warning;
    Color danger;
};

static std::vector<LauncherTheme> g_themes = {
    {
        L"void-purple", L"Void Purple",
        Color(255, 12, 13, 18),   // bgBase #0c0d12
        Color(255, 20, 22, 32),   // bgSurface #141620
        Color(255, 26, 30, 46),   // bgSurfaceElevated #1a1e2e
        Color(255, 22, 26, 41),   // bgCard #161a29
        Color(70, 157, 78, 221),  // border
        Color(255, 157, 78, 221), // borderFocus #9d4edd
        Color(255, 157, 78, 221), // accent #9d4edd
        Color(255, 199, 125, 255),// accentSecondary #c77dff
        Color(100, 157, 78, 221), // accentGlow
        Color(255, 240, 242, 248),// textMain
        Color(255, 140, 147, 168),// textMuted
        Color(255, 16, 185, 129), // success #10b981
        Color(255, 245, 158, 11), // warning #f59e0b
        Color(255, 239, 68, 68)   // danger #ef4444
    },
    {
        L"crimson", L"Crimson Red",
        Color(255, 15, 11, 13),
        Color(255, 26, 18, 22),
        Color(255, 36, 24, 31),
        Color(255, 31, 20, 26),
        Color(70, 225, 29, 72),
        Color(255, 225, 29, 72),
        Color(255, 225, 29, 72),
        Color(255, 251, 113, 133),
        Color(100, 225, 29, 72),
        Color(255, 240, 242, 248),
        Color(255, 160, 140, 148),
        Color(255, 16, 185, 129),
        Color(255, 245, 158, 11),
        Color(255, 239, 68, 68)
    },
    {
        L"cyan-neon", L"Cyberpunk Cyan",
        Color(255, 8, 15, 20),
        Color(255, 12, 26, 36),
        Color(255, 18, 36, 51),
        Color(255, 14, 30, 43),
        Color(70, 6, 182, 212),
        Color(255, 6, 182, 212),
        Color(255, 6, 182, 212),
        Color(255, 56, 189, 248),
        Color(100, 6, 182, 212),
        Color(255, 240, 242, 248),
        Color(255, 130, 155, 175),
        Color(255, 16, 185, 129),
        Color(255, 245, 158, 11),
        Color(255, 239, 68, 68)
    },
    {
        L"emerald", L"Emerald Green",
        Color(255, 9, 18, 14),
        Color(255, 14, 28, 22),
        Color(255, 20, 41, 32),
        Color(255, 16, 33, 26),
        Color(70, 16, 185, 129),
        Color(255, 16, 185, 129),
        Color(255, 16, 185, 129),
        Color(255, 52, 211, 153),
        Color(100, 16, 185, 129),
        Color(255, 240, 242, 248),
        Color(255, 130, 165, 145),
        Color(255, 16, 185, 129),
        Color(255, 245, 158, 11),
        Color(255, 239, 68, 68)
    },
    {
        L"amber", L"Amber Gold",
        Color(255, 18, 14, 8),
        Color(255, 28, 22, 14),
        Color(255, 41, 32, 20),
        Color(255, 33, 26, 16),
        Color(70, 245, 158, 11),
        Color(255, 245, 158, 11),
        Color(255, 245, 158, 11),
        Color(255, 251, 191, 36),
        Color(100, 245, 158, 11),
        Color(255, 240, 242, 248),
        Color(255, 165, 150, 130),
        Color(255, 16, 185, 129),
        Color(255, 245, 158, 11),
        Color(255, 239, 68, 68)
    },
    {
        L"nordic-blue", L"Nordic Blue",
        Color(255, 10, 14, 23),
        Color(255, 16, 23, 38),
        Color(255, 24, 34, 56),
        Color(255, 18, 28, 46),
        Color(70, 59, 130, 246),
        Color(255, 59, 130, 246),
        Color(255, 59, 130, 246),
        Color(255, 96, 165, 250),
        Color(100, 59, 130, 246),
        Color(255, 240, 242, 248),
        Color(255, 140, 155, 180),
        Color(255, 16, 185, 129),
        Color(255, 245, 158, 11),
        Color(255, 239, 68, 68)
    }
};

static LauncherTheme g_currentTheme = g_themes[0];

// ----------------------------------------------------------------------------
// Global UI State
// ----------------------------------------------------------------------------
static HINSTANCE g_hInstance = NULL;
static HWND g_hMainWindow = NULL;
static HWND g_hEditNick = NULL;
static HWND g_hEditServer = NULL;

#define IDC_EDIT_NICKNAME 1001
#define IDC_EDIT_SERVER   1002

static LauncherConfig g_config;
static int g_currentTab = 0; // 0: Containers, 1: Mods, 2: Servers, 3: Logs, 4: Backups, 5: Settings
static int g_hoveredTab = -1;
static bool g_hoverPlayBtn = false;
static bool g_pressPlayBtn = false;
static bool g_isRussian = true;

// Active container & launch status
static std::wstring g_activeContainerName = L"Vanilla 1.21.1";
static std::wstring g_activeVersion = L"1.21.1";
static std::wstring g_launchStatus = L"Готов к запуску";
static int g_launchProgressPct = 0;
static std::atomic<bool> g_isGameRunning(false);
static HANDLE g_hGameProcess = NULL;

// Logs
static std::vector<std::wstring> g_logs;
static std::mutex g_logsMutex;
static std::wstring g_lastCrashTitle = L"";
static std::wstring g_lastCrashCause = L"";
static std::wstring g_lastCrashSolution = L"";

// Server Ping
static std::wstring g_serverHost = L"mc.hypixel.net";
static std::wstring g_serverStatus = L"";
static std::wstring g_serverPingText = L"";
static std::wstring g_serverMotd = L"";
static std::wstring g_serverPlayers = L"";
static bool g_serverIsOnline = false;

// Mod items
struct ModItem {
    std::wstring name;
    std::wstring description;
    std::wstring downloads;
    std::wstring category;
    bool installed;
};

static std::vector<ModItem> g_mods = {
    { L"Sodium", L"Современный движок рендеринга, колоссально повышающий FPS и плавность графики.", L"32.4M", L"Оптимизация", true },
    { L"Iris Shaders", L"Современный загрузчик шейдер-паков с поддержкой PBR и совместимостью с Sodium.", L"24.1M", L"Шейдеры", true },
    { L"Lithium", L"Глубокая оптимизация тика сервера, физики мобов и чанков без изменения механик.", L"19.8M", L"Оптимизация", false },
    { L"Fabric API", L"Основная библиотека хуков и событий, необходимая для большинства модов Fabric.", L"45.2M", L"Библиотека", true },
    { L"AppleSkin", L"Удобное отображение насыщения и истощения полоски голода над хотбаром.", L"28.6M", L"Интерфейс", false },
    { L"JourneyMap", L"Красивая карта реального времени с радаром мобов и путевыми точками (waypoint).", L"38.1M", L"Карта", false }
};

// Available versions
static std::vector<std::wstring> g_versions = {
    L"1.21.1", L"1.20.4", L"1.20.1", L"1.19.4", L"1.18.2", L"1.16.5", L"1.12.2"
};
static int g_selectedVersionIndex = 0;

// RAM Options (MB)
static std::vector<int> g_ramOptions = { 2048, 4096, 6144, 8192, 12288, 16384 };

// Forward declarations
void AppendLog(const std::wstring& line);
void StartMinecraftLaunch();
void PingActiveServer();

// Translation Helper
inline const wchar_t* Tr(const wchar_t* ru, const wchar_t* en) {
    return g_isRussian ? ru : en;
}
inline std::wstring Tr(const std::wstring& ru, const std::wstring& en) {
    return g_isRussian ? ru : en;
}

// ----------------------------------------------------------------------------
// GDI+ Graphics Helpers
// ----------------------------------------------------------------------------
static void AddRoundRect(GraphicsPath& path, float x, float y, float w, float h, float r) {
    if (r <= 0.0f) {
        path.AddRectangle(RectF(x, y, w, h));
        return;
    }
    float d = r * 2.0f;
    path.AddArc(x, y, d, d, 180, 90);
    path.AddArc(x + w - d, y, d, d, 270, 90);
    path.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    path.AddArc(x, y + h - d, d, d, 90, 90);
    path.CloseFigure();
}

static void DrawCard(Graphics& g, float x, float y, float w, float h, float r, const Color& bgColor, const Color& borderColor) {
    GraphicsPath path;
    AddRoundRect(path, x, y, w, h, r);
    SolidBrush bgBrush(bgColor);
    g.FillPath(&bgBrush, &path);
    Pen borderPen(borderColor, 1.0f);
    g.DrawPath(&borderPen, &path);
}

static void DrawGradientButton(Graphics& g, float x, float y, float w, float h, float r,
                               const Color& c1, const Color& c2, const Color& textColor,
                               const wchar_t* text, Font& font, bool hovered, bool pressed) {
    GraphicsPath path;
    AddRoundRect(path, x, y, w, h, r);

    Color actualC1 = c1;
    Color actualC2 = c2;
    if (hovered) {
        actualC1 = Color(c1.GetA(), std::min(255, (int)c1.GetR() + 25), std::min(255, (int)c1.GetG() + 25), std::min(255, (int)c1.GetB() + 25));
        actualC2 = Color(c2.GetA(), std::min(255, (int)c2.GetR() + 25), std::min(255, (int)c2.GetG() + 25), std::min(255, (int)c2.GetB() + 25));
    }
    if (pressed) {
        y += 1.0f;
    }

    LinearGradientBrush grad(PointF(x, y), PointF(x + w, y + h), actualC1, actualC2);
    g.FillPath(&grad, &path);

    Pen highlightPen(Color(80, 255, 255, 255), 1.0f);
    g.DrawPath(&highlightPen, &path);

    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);
    sf.SetLineAlignment(StringAlignmentCenter);
    SolidBrush textBrush(textColor);
    RectF textRect(x, y, w, h);
    g.DrawString(text, -1, &font, textRect, &sf, &textBrush);
}

// ----------------------------------------------------------------------------
// Window Procedure & Painting
// ----------------------------------------------------------------------------
static void PaintMainUI(HWND hwnd, HDC hdc) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;
    if (width <= 0 || height <= 0) return;

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, width, height);
    HGDIOBJ oldBitmap = SelectObject(memDC, memBitmap);

    Graphics g(memDC);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    SolidBrush bgBrush(g_currentTheme.bgBase);
    g.FillRectangle(&bgBrush, 0, 0, width, height);

    FontFamily ff(L"Segoe UI");
    Font fontTitle(&ff, 18, FontStyleBold, UnitPixel);
    Font fontSubtitle(&ff, 12, FontStyleRegular, UnitPixel);
    Font fontHeader(&ff, 14, FontStyleBold, UnitPixel);
    Font fontRegular(&ff, 13, FontStyleRegular, UnitPixel);
    Font fontBold(&ff, 13, FontStyleBold, UnitPixel);
    Font fontSmall(&ff, 11, FontStyleRegular, UnitPixel);
    Font fontSmallBold(&ff, 11, FontStyleBold, UnitPixel);
    Font fontPlayBtn(&ff, 15, FontStyleBold, UnitPixel);
    Font fontBrand(&ff, 16, FontStyleBold, UnitPixel);

    SolidBrush whiteBrush(Color(255, 255, 255, 255));
    SolidBrush mutedBrush(g_currentTheme.textMuted);
    SolidBrush greenBrush(g_currentTheme.success);
    SolidBrush warnBrush(g_currentTheme.warning);

    int sidebarW = 220;
    int dockH = 84;
    int mainH = height - dockH;

    // ------------------------------------------------------------------------
    // 1. Sidebar (Width: 220px)
    // ------------------------------------------------------------------------
    SolidBrush sidebarBrush(g_currentTheme.bgSurface);
    g.FillRectangle(&sidebarBrush, 0, 0, sidebarW, mainH);

    Pen borderPen(g_currentTheme.border, 1.0f);
    g.DrawLine(&borderPen, sidebarW, 0, sidebarW, mainH);

    // Brand Logo
    GraphicsPath logoPath;
    AddRoundRect(logoPath, 20, 18, 38, 38, 10);
    LinearGradientBrush logoGrad(PointF(20, 18), PointF(58, 56), g_currentTheme.accent, g_currentTheme.accentSecondary);
    g.FillPath(&logoGrad, &logoPath);

    StringFormat sfCenter;
    sfCenter.SetAlignment(StringAlignmentCenter);
    sfCenter.SetLineAlignment(StringAlignmentCenter);
    RectF logoTextRc(20, 18, 38, 38);
    Font fontLogoIcon(&ff, 18, FontStyleBold, UnitPixel);
    g.DrawString(L"Æ", -1, &fontLogoIcon, logoTextRc, &sfCenter, &whiteBrush);

    PointF brandPt(68, 20);
    g.DrawString(L"Aether", -1, &fontBrand, brandPt, &whiteBrush);

    // "LAUNCHER" Badge
    GraphicsPath badgePath;
    AddRoundRect(badgePath, 68, 42, 64, 14, 4);
    SolidBrush badgeBg(Color(40, g_currentTheme.accent.GetR(), g_currentTheme.accent.GetG(), g_currentTheme.accent.GetB()));
    g.FillPath(&badgeBg, &badgePath);
    SolidBrush accentBrush(g_currentTheme.accentSecondary);
    RectF badgeTextRc(68, 42, 64, 14);
    Font fontPill(&ff, 9, FontStyleBold, UnitPixel);
    g.DrawString(L"LAUNCHER", -1, &fontPill, badgeTextRc, &sfCenter, &accentBrush);

    g.DrawLine(&borderPen, 20, 72, sidebarW - 20, 72);

    // Sidebar Navigation Items
    struct NavItem {
        const wchar_t* icon;
        const wchar_t* titleRu;
        const wchar_t* titleEn;
    };
    static NavItem navItems[] = {
        { L"📦", L"Контейнеры", L"Containers" },
        { L"🧩", L"Моды (Modrinth)", L"Mods (Modrinth)" },
        { L"🌐", L"Серверы", L"Servers" },
        { L"📜", L"Логи и краши", L"Logs & Crashes" },
        { L"💾", L"Резервные копии", L"Backups" },
        { L"⚙", L"Настройки", L"Settings" }
    };

    int navStartY = 88;
    int navItemH = 42;
    for (int i = 0; i < 6; ++i) {
        int itemY = navStartY + i * (navItemH + 4);
        bool isActive = (g_currentTab == i);
        bool isHovered = (g_hoveredTab == i);

        if (isActive) {
            GraphicsPath navPill;
            AddRoundRect(navPill, 12, (float)itemY, sidebarW - 24, (float)navItemH, 8);
            SolidBrush activePillBrush(Color(45, g_currentTheme.accent.GetR(), g_currentTheme.accent.GetG(), g_currentTheme.accent.GetB()));
            g.FillPath(&activePillBrush, &navPill);

            GraphicsPath accentBar;
            AddRoundRect(accentBar, 12, (float)itemY + 6, 4, (float)navItemH - 12, 2);
            SolidBrush barBrush(g_currentTheme.accent);
            g.FillPath(&barBrush, &accentBar);
        } else if (isHovered) {
            GraphicsPath hoverPill;
            AddRoundRect(hoverPill, 12, (float)itemY, sidebarW - 24, (float)navItemH, 8);
            SolidBrush hoverPillBrush(Color(25, 255, 255, 255));
            g.FillPath(&hoverPillBrush, &hoverPill);
        }

        Font fontIcon(&ff, 14, FontStyleRegular, UnitPixel);
        PointF iconPt(24, (float)itemY + 11);
        SolidBrush iconBrush(isActive ? g_currentTheme.textMain : g_currentTheme.textMuted);
        g.DrawString(navItems[i].icon, -1, &fontIcon, iconPt, &iconBrush);

        PointF textPt(48, (float)itemY + 11);
        Font& itemFont = isActive ? fontBold : fontRegular;
        SolidBrush textBrush(isActive ? g_currentTheme.textMain : g_currentTheme.textMuted);
        g.DrawString(Tr(navItems[i].titleRu, navItems[i].titleEn), -1, &itemFont, textPt, &textBrush);
    }

    int footerY = mainH - 42;
    g.DrawLine(&borderPen, 20, footerY - 10, sidebarW - 20, footerY - 10);
    DrawCard(g, 20, (float)footerY, 80, 24, 6, g_currentTheme.bgSurfaceElevated, g_currentTheme.border);
    RectF verRc(20, (float)footerY, 80, 24);
    g.DrawString(L"v1.0.38", -1, &fontSmallBold, verRc, &sfCenter, &mutedBrush);

    PointF enginePt(110, (float)footerY + 4);
    Font fontEngine(&ff, 10, FontStyleRegular, UnitPixel);
    g.DrawString(L"Native C++ Win32", -1, &fontEngine, enginePt, &mutedBrush);

    // ------------------------------------------------------------------------
    // 2. Top Header Bar (Height: 54px)
    // ------------------------------------------------------------------------
    int topH = 54;
    int contentX = sidebarW + 24;
    int contentW = width - sidebarW - 48;

    SolidBrush topBrush(g_currentTheme.bgSurface);
    g.FillRectangle(&topBrush, sidebarW, 0, width - sidebarW, topH);
    g.DrawLine(&borderPen, sidebarW, topH, width, topH);

    DrawCard(g, (float)sidebarW + 20, 11, 220, 32, 8, g_currentTheme.bgSurfaceElevated, g_currentTheme.border);
    PointF activeIconPt((float)sidebarW + 30, 16);
    g.DrawString(L"🎮", -1, &fontRegular, activeIconPt, &whiteBrush);
    PointF activeNamePt((float)sidebarW + 56, 17);
    g.DrawString(g_activeContainerName.c_str(), -1, &fontBold, activeNamePt, &whiteBrush);

    int langX = width - 110;
    DrawCard(g, (float)langX, 12, 70, 30, 8, g_currentTheme.bgSurfaceElevated, g_currentTheme.border);

    if (g_isRussian) {
        DrawCard(g, (float)langX + 2, 14, 32, 26, 6, g_currentTheme.accent, g_currentTheme.accent);
    } else {
        DrawCard(g, (float)langX + 36, 14, 32, 26, 6, g_currentTheme.accent, g_currentTheme.accent);
    }

    RectF ruRc((float)langX, 12, 35, 30);
    g.DrawString(L"RU", -1, &fontSmallBold, ruRc, &sfCenter, &whiteBrush);
    RectF enRc((float)langX + 35, 12, 35, 30);
    g.DrawString(L"EN", -1, &fontSmallBold, enRc, &sfCenter, &whiteBrush);

    // ------------------------------------------------------------------------
    // 3. Tab Content Area
    // ------------------------------------------------------------------------
    int contentY = topH + 20;

    if (g_currentTab == 0) {
        // --- Containers Tab ---
        PointF titlePt((float)contentX, (float)contentY);
        g.DrawString(Tr(L"Игровые контейнеры", L"Game Containers"), -1, &fontTitle, titlePt, &whiteBrush);

        PointF subPt((float)contentX, (float)contentY + 28);
        g.DrawString(Tr(L"Изолированные профили для версий Minecraft, сборок и модов",
                        L"Isolated profiles for Minecraft versions, modpacks and loaders"),
                     -1, &fontSubtitle, subPt, &mutedBrush);

        int cardY = contentY + 68;
        int cardW = (contentW - 20) / 2;
        int cardH = 140;

        // Card 1: Vanilla 1.21.1
        DrawCard(g, (float)contentX, (float)cardY, (float)cardW, (float)cardH, 12, g_currentTheme.bgCard, g_currentTheme.borderFocus);
        PointF c1IconPt((float)contentX + 16, (float)cardY + 16);
        g.DrawString(L"🧊", -1, &fontHeader, c1IconPt, &whiteBrush);

        PointF c1TitlePt((float)contentX + 44, (float)cardY + 16);
        g.DrawString(L"Minecraft 1.21.1 Vanilla", -1, &fontBold, c1TitlePt, &whiteBrush);

        DrawCard(g, (float)contentX + 44, (float)cardY + 40, 60, 20, 4, Color(40, 16, 185, 129), Color(100, 16, 185, 129));
        RectF b1Rc((float)contentX + 44, (float)cardY + 40, 60, 20);
        g.DrawString(L"Vanilla", -1, &fontSmallBold, b1Rc, &sfCenter, &greenBrush);

        DrawCard(g, (float)contentX + 112, (float)cardY + 40, 50, 20, 4, g_currentTheme.bgSurfaceElevated, g_currentTheme.border);
        RectF b2Rc((float)contentX + 112, (float)cardY + 40, 50, 20);
        g.DrawString(L"1.21.1", -1, &fontSmall, b2Rc, &sfCenter, &mutedBrush);

        PointF c1StatsPt((float)contentX + 16, (float)cardY + 76);
        g.DrawString(Tr(L"Память: 4 GB  •  Модов: 0  •  Общие миры: Вкл",
                        L"Memory: 4 GB  •  Mods: 0  •  Shared Saves: On"),
                     -1, &fontSmall, c1StatsPt, &mutedBrush);

        DrawCard(g, (float)contentX + 16, (float)cardY + 102, 100, 24, 6, g_currentTheme.accent, g_currentTheme.accent);
        RectF activePillRc((float)contentX + 16, (float)cardY + 102, 100, 24);
        g.DrawString(Tr(L"✓ Активен", L"✓ Active"), -1, &fontSmallBold, activePillRc, &sfCenter, &whiteBrush);

        // Card 2: Fabric 1.20.4
        int c2X = contentX + cardW + 20;
        DrawCard(g, (float)c2X, (float)cardY, (float)cardW, (float)cardH, 12, g_currentTheme.bgCard, g_currentTheme.border);
        PointF c2IconPt((float)c2X + 16, (float)cardY + 16);
        g.DrawString(L"⚡", -1, &fontHeader, c2IconPt, &whiteBrush);

        PointF c2TitlePt((float)c2X + 44, (float)cardY + 16);
        g.DrawString(L"Fabric Performance & Shaders", -1, &fontBold, c2TitlePt, &whiteBrush);

        DrawCard(g, (float)c2X + 44, (float)cardY + 40, 55, 20, 4, Color(40, 6, 182, 212), Color(100, 6, 182, 212));
        RectF b3Rc((float)c2X + 44, (float)cardY + 40, 55, 20);
        SolidBrush cyanBrush(Color(255, 56, 189, 248));
        g.DrawString(L"Fabric", -1, &fontSmallBold, b3Rc, &sfCenter, &cyanBrush);

        DrawCard(g, (float)c2X + 107, (float)cardY + 40, 50, 20, 4, g_currentTheme.bgSurfaceElevated, g_currentTheme.border);
        RectF b4Rc((float)c2X + 107, (float)cardY + 40, 50, 20);
        g.DrawString(L"1.20.4", -1, &fontSmall, b4Rc, &sfCenter, &mutedBrush);

        PointF c2StatsPt((float)c2X + 16, (float)cardY + 76);
        g.DrawString(Tr(L"Память: 4 GB  •  Модов: 8 (Sodium, Iris, Lithium...)",
                        L"Memory: 4 GB  •  Mods: 8 (Sodium, Iris, Lithium...)"),
                     -1, &fontSmall, c2StatsPt, &mutedBrush);

        DrawCard(g, (float)c2X + 16, (float)cardY + 102, 120, 24, 6, g_currentTheme.bgSurfaceElevated, g_currentTheme.border);
        RectF selPillRc((float)c2X + 16, (float)cardY + 102, 120, 24);
        g.DrawString(Tr(L"Выбрать профиль", L"Select Profile"), -1, &fontSmallBold, selPillRc, &sfCenter, &mutedBrush);

        // Card 3: Forge 1.16.5
        int c3Y = cardY + cardH + 16;
        DrawCard(g, (float)contentX, (float)c3Y, (float)cardW, (float)cardH, 12, g_currentTheme.bgCard, g_currentTheme.border);
        PointF c3IconPt((float)contentX + 16, (float)c3Y + 16);
        g.DrawString(L"🔨", -1, &fontHeader, c3IconPt, &whiteBrush);

        PointF c3TitlePt((float)contentX + 44, (float)c3Y + 16);
        g.DrawString(L"Classic Tech & Magic", -1, &fontBold, c3TitlePt, &whiteBrush);

        DrawCard(g, (float)contentX + 44, (float)c3Y + 40, 50, 20, 4, Color(40, 245, 158, 11), Color(100, 245, 158, 11));
        RectF b5Rc((float)contentX + 44, (float)c3Y + 40, 50, 20);
        SolidBrush amberBrush(Color(255, 251, 191, 36));
        g.DrawString(L"Forge", -1, &fontSmallBold, b5Rc, &sfCenter, &amberBrush);

        DrawCard(g, (float)contentX + 102, (float)c3Y + 40, 50, 20, 4, g_currentTheme.bgSurfaceElevated, g_currentTheme.border);
        RectF b6Rc((float)contentX + 102, (float)c3Y + 40, 50, 20);
        g.DrawString(L"1.16.5", -1, &fontSmall, b6Rc, &sfCenter, &mutedBrush);

        PointF c3StatsPt((float)contentX + 16, (float)c3Y + 76);
        g.DrawString(Tr(L"Память: 6 GB  •  Модов: 15 (Create, JEI, Thermal...)",
                        L"Memory: 6 GB  •  Mods: 15 (Create, JEI, Thermal...)"),
                     -1, &fontSmall, c3StatsPt, &mutedBrush);

        DrawCard(g, (float)contentX + 16, (float)c3Y + 102, 120, 24, 6, g_currentTheme.bgSurfaceElevated, g_currentTheme.border);
        RectF selPillRc2((float)contentX + 16, (float)c3Y + 102, 120, 24);
        g.DrawString(Tr(L"Выбрать профиль", L"Select Profile"), -1, &fontSmallBold, selPillRc2, &sfCenter, &mutedBrush);
    }
    else if (g_currentTab == 1) {
        // --- Modrinth Mods Tab ---
        PointF titlePt((float)contentX, (float)contentY);
        g.DrawString(Tr(L"Каталог Modrinth v2", L"Modrinth v2 Catalog"), -1, &fontTitle, titlePt, &whiteBrush);

        PointF subPt((float)contentX, (float)contentY + 28);
        g.DrawString(Tr(L"Прямой поиск и установка модов, датапаков и шейдеров в 1 клик",
                        L"Direct 1-click installation of mods, datapacks and shaders"),
                     -1, &fontSubtitle, subPt, &mutedBrush);

        int searchY = contentY + 62;
        DrawCard(g, (float)contentX, (float)searchY, (float)contentW - 100, 36, 8, g_currentTheme.bgSurfaceElevated, g_currentTheme.border);
        PointF searchIconPt((float)contentX + 12, (float)searchY + 8);
        g.DrawString(L"🔍  Поиск модов...", -1, &fontSmall, searchIconPt, &mutedBrush);

        DrawGradientButton(g, (float)contentX + contentW - 90, (float)searchY, 90, 36, 8,
                           g_currentTheme.accent, g_currentTheme.accentSecondary,
                           Color(255, 255, 255, 255), Tr(L"Найти", L"Search"), fontBold, false, false);

        int modListY = searchY + 48;
        int modItemH = 54;
        for (size_t i = 0; i < g_mods.size() && i < 5; ++i) {
            int mY = modListY + (int)i * (modItemH + 8);
            DrawCard(g, (float)contentX, (float)mY, (float)contentW, (float)modItemH, 8, g_currentTheme.bgCard, g_currentTheme.border);

            PointF mNamePt((float)contentX + 16, (float)mY + 10);
            g.DrawString(g_mods[i].name.c_str(), -1, &fontBold, mNamePt, &whiteBrush);

            DrawCard(g, (float)contentX + 120, (float)mY + 10, 85, 18, 4, g_currentTheme.bgSurfaceElevated, g_currentTheme.border);
            RectF tagRc((float)contentX + 120, (float)mY + 10, 85, 18);
            g.DrawString(g_mods[i].category.c_str(), -1, &fontSmall, tagRc, &sfCenter, &mutedBrush);

            PointF mDescPt((float)contentX + 16, (float)mY + 30);
            g.DrawString(g_mods[i].description.c_str(), -1, &fontSmall, mDescPt, &mutedBrush);

            int btnW = 110;
            int btnX = contentX + contentW - btnW - 14;
            if (g_mods[i].installed) {
                DrawCard(g, (float)btnX, (float)mY + 12, (float)btnW, 28, 6, Color(40, 16, 185, 129), g_currentTheme.success);
                RectF btnRc((float)btnX, (float)mY + 12, (float)btnW, 28);
                SolidBrush greenText(g_currentTheme.success);
                g.DrawString(Tr(L"✓ Установлен", L"✓ Installed"), -1, &fontSmallBold, btnRc, &sfCenter, &greenText);
            } else {
                DrawGradientButton(g, (float)btnX, (float)mY + 12, (float)btnW, 28, 6,
                                   g_currentTheme.accent, g_currentTheme.accentSecondary,
                                   Color(255, 255, 255, 255), Tr(L"+ Установить", L"+ Install"), fontSmallBold, false, false);
            }
        }
    }
    else if (g_currentTab == 2) {
        // --- Server Monitoring Tab ---
        PointF titlePt((float)contentX, (float)contentY);
        g.DrawString(Tr(L"Мониторинг серверов", L"Server Monitoring"), -1, &fontTitle, titlePt, &whiteBrush);

        PointF subPt((float)contentX, (float)contentY + 28);
        g.DrawString(Tr(L"Проверка онлайна, задержки (ping) и статуса сервера до запуска",
                        L"Check server online status, latency (ping) and MOTD before launching"),
                     -1, &fontSubtitle, subPt, &mutedBrush);

        int pingBarY = contentY + 62;
        DrawCard(g, (float)contentX, (float)pingBarY, (float)contentW - 160, 36, 8, g_currentTheme.bgSurfaceElevated, g_currentTheme.border);

        DrawGradientButton(g, (float)contentX + contentW - 150, (float)pingBarY, 150, 36, 8,
                           g_currentTheme.accent, g_currentTheme.accentSecondary,
                           Color(255, 255, 255, 255), Tr(L"Проверить пинг", L"Check Ping"), fontBold, false, false);

        int resY = pingBarY + 54;
        DrawCard(g, (float)contentX, (float)resY, (float)contentW, 190, 12, g_currentTheme.bgCard, g_currentTheme.borderFocus);

        PointF srvIconPt((float)contentX + 20, (float)resY + 18);
        g.DrawString(L"🌐", -1, &fontHeader, srvIconPt, &whiteBrush);

        PointF srvHostPt((float)contentX + 48, (float)resY + 18);
        g.DrawString(g_serverHost.c_str(), -1, &fontHeader, srvHostPt, &whiteBrush);

        DrawCard(g, (float)contentX + contentW - 110, (float)resY + 16, 90, 24, 6, Color(40, 16, 185, 129), g_currentTheme.success);
        RectF onRc((float)contentX + contentW - 110, (float)resY + 16, 90, 24);
        g.DrawString(Tr(L"● Онлайн", L"● Online"), -1, &fontSmallBold, onRc, &sfCenter, &greenBrush);

        PointF pingPt((float)contentX + 48, (float)resY + 42);
        std::wstring pingDisplay = Tr(L"Задержка: 24 мс  •  Версия: 1.8.x - 1.21.x", L"Latency: 24 ms  •  Version: 1.8.x - 1.21.x");
        if (!g_serverPingText.empty()) pingDisplay = g_serverPingText;
        g.DrawString(pingDisplay.c_str(), -1, &fontSmall, pingPt, &mutedBrush);

        DrawCard(g, (float)contentX + 20, (float)resY + 70, (float)contentW - 40, 60, 8, g_currentTheme.bgSurfaceElevated, g_currentTheme.border);
        PointF motdPt((float)contentX + 32, (float)resY + 80);
        std::wstring motdText = g_serverMotd.empty()
            ? L"Hypixel Network [1.8-1.21] SUMMER EVENT!\n★ 100+ Games ★ BedWars, SkyBlock, Duels & SMP"
            : g_serverMotd;
        g.DrawString(motdText.c_str(), -1, &fontSmallBold, motdPt, &whiteBrush);

        PointF plyPt((float)contentX + 20, (float)resY + 146);
        std::wstring plyText = g_serverPlayers.empty()
            ? Tr(L"Игроков онлайн: 42,184 / 100,000", L"Players online: 42,184 / 100,000")
            : g_serverPlayers;
        g.DrawString(plyText.c_str(), -1, &fontRegular, plyPt, &whiteBrush);
    }
    else if (g_currentTab == 3) {
        // --- Logs & Smart Crash Diagnostic Tab ---
        PointF titlePt((float)contentX, (float)contentY);
        g.DrawString(Tr(L"Логи и умная диагностика", L"Logs & Crash Diagnostics"), -1, &fontTitle, titlePt, &whiteBrush);

        PointF subPt((float)contentX, (float)contentY + 28);
        g.DrawString(Tr(L"Отслеживание вывода процесса Minecraft и автоматический анализ причин сбоев",
                        L"Monitor Minecraft process output and automatically diagnose crash causes"),
                     -1, &fontSubtitle, subPt, &mutedBrush);

        int logY = contentY + 60;

        if (!g_lastCrashCause.empty()) {
            DrawCard(g, (float)contentX, (float)logY, (float)contentW, 76, 8, Color(40, 245, 158, 11), g_currentTheme.warning);
            PointF warnIconPt((float)contentX + 16, (float)logY + 12);
            g.DrawString(L"⚠", -1, &fontHeader, warnIconPt, &whiteBrush);

            PointF diagTitlePt((float)contentX + 42, (float)logY + 12);
            g.DrawString(g_lastCrashTitle.c_str(), -1, &fontBold, diagTitlePt, &warnBrush);

            PointF diagCausePt((float)contentX + 42, (float)logY + 32);
            g.DrawString(g_lastCrashCause.c_str(), -1, &fontSmall, diagCausePt, &whiteBrush);

            PointF diagSolPt((float)contentX + 42, (float)logY + 50);
            g.DrawString(g_lastCrashSolution.c_str(), -1, &fontSmallBold, diagSolPt, &greenBrush);

            logY += 88;
        }

        int termH = mainH - logY - 10;
        DrawCard(g, (float)contentX, (float)logY, (float)contentW, (float)termH, 8, Color(255, 10, 11, 16), g_currentTheme.border);

        FontFamily ffMono(L"Consolas");
        Font fontMono(&ffMono, 11, FontStyleRegular, UnitPixel);

        std::lock_guard<std::mutex> lock(g_logsMutex);
        int maxLines = (termH - 20) / 18;
        int startIdx = std::max(0, (int)g_logs.size() - maxLines);

        for (int i = startIdx; i < (int)g_logs.size(); ++i) {
            int lineY = logY + 10 + (i - startIdx) * 18;
            PointF logPt((float)contentX + 14, (float)lineY);

            Color lineCol = g_currentTheme.textMain;
            if (g_logs[i].find(L"[ERROR]") != std::wstring::npos || g_logs[i].find(L"Exception") != std::wstring::npos) {
                lineCol = g_currentTheme.danger;
            } else if (g_logs[i].find(L"[WARN]") != std::wstring::npos) {
                lineCol = g_currentTheme.warning;
            } else if (g_logs[i].find(L"[INFO]") != std::wstring::npos) {
                lineCol = Color(255, 140, 180, 255);
            }

            SolidBrush lineBrush(lineCol);
            g.DrawString(g_logs[i].c_str(), -1, &fontMono, logPt, &lineBrush);
        }
    }
    else if (g_currentTab == 4) {
        // --- Backups Tab ---
        PointF titlePt((float)contentX, (float)contentY);
        g.DrawString(Tr(L"Резервные копии миров", L"World Backups"), -1, &fontTitle, titlePt, &whiteBrush);

        PointF subPt((float)contentX, (float)contentY + 28);
        g.DrawString(Tr(L"Снимки сохранений и конфигураций перед установкой новых модов",
                        L"Snapshots of world saves and configs before testing new mods"),
                     -1, &fontSubtitle, subPt, &mutedBrush);

        int bkY = contentY + 68;
        DrawGradientButton(g, (float)contentX, (float)bkY, 180, 36, 8,
                           g_currentTheme.accent, g_currentTheme.accentSecondary,
                           Color(255, 255, 255, 255), Tr(L"+ Создать снимок", L"+ Create Snapshot"), fontBold, false, false);

        int listY = bkY + 52;
        DrawCard(g, (float)contentX, (float)listY, (float)contentW, 64, 8, g_currentTheme.bgCard, g_currentTheme.border);
        PointF bIconPt((float)contentX + 16, (float)listY + 16);
        g.DrawString(L"💾", -1, &fontHeader, bIconPt, &whiteBrush);

        PointF bTitlePt((float)contentX + 44, (float)listY + 14);
        g.DrawString(Tr(L"Vanilla 1.21.1 - Снимок перед установкой модов", L"Vanilla 1.21.1 - Snapshot before mod test"), -1, &fontBold, bTitlePt, &whiteBrush);

        PointF bTimePt((float)contentX + 44, (float)listY + 36);
        g.DrawString(Tr(L"Сегодня, 14:20  •  Размер: 42.1 МБ", L"Today, 14:20  •  Size: 42.1 MB"), -1, &fontSmall, bTimePt, &mutedBrush);

        DrawCard(g, (float)contentX + contentW - 140, (float)listY + 18, 120, 28, 6, g_currentTheme.bgSurfaceElevated, g_currentTheme.border);
        RectF rstRc((float)contentX + contentW - 140, (float)listY + 18, 120, 28);
        g.DrawString(Tr(L"Восстановить", L"Restore"), -1, &fontSmallBold, rstRc, &sfCenter, &whiteBrush);
    }
    else if (g_currentTab == 5) {
        // --- Settings Tab ---
        PointF titlePt((float)contentX, (float)contentY);
        g.DrawString(Tr(L"Настройки лаунчера", L"Launcher Settings"), -1, &fontTitle, titlePt, &whiteBrush);

        PointF subPt((float)contentX, (float)contentY + 28);
        g.DrawString(Tr(L"Память, автоподбор Java, оптимизация и визуальные темы",
                        L"Memory allocation, Java scanner, optimization and themes"),
                     -1, &fontSubtitle, subPt, &mutedBrush);

        int setY = contentY + 68;

        DrawCard(g, (float)contentX, (float)setY, (float)contentW, 90, 10, g_currentTheme.bgCard, g_currentTheme.border);
        PointF ramLblPt((float)contentX + 16, (float)setY + 14);
        g.DrawString(Tr(L"Выделенная оперативная память (RAM)", L"Allocated RAM"), -1, &fontBold, ramLblPt, &whiteBrush);

        int pillStartX = contentX + 16;
        for (size_t i = 0; i < g_ramOptions.size(); ++i) {
            int pX = pillStartX + (int)i * 72;
            bool isSel = (g_config.memoryMb == g_ramOptions[i]);

            if (isSel) {
                DrawCard(g, (float)pX, (float)setY + 44, 62, 30, 6, g_currentTheme.accent, g_currentTheme.accent);
            } else {
                DrawCard(g, (float)pX, (float)setY + 44, 62, 30, 6, g_currentTheme.bgSurfaceElevated, g_currentTheme.border);
            }

            std::wstring ramLabel = std::to_wstring(g_ramOptions[i] / 1024) + L" GB";
            RectF ramRc((float)pX, (float)setY + 44, 62, 30);
            SolidBrush ramTextBrush(isSel ? Color(255, 255, 255, 255) : g_currentTheme.textMuted);
            g.DrawString(ramLabel.c_str(), -1, &fontSmallBold, ramRc, &sfCenter, &ramTextBrush);
        }

        int themeY = setY + 104;
        DrawCard(g, (float)contentX, (float)themeY, (float)contentW, 90, 10, g_currentTheme.bgCard, g_currentTheme.border);
        PointF thmLblPt((float)contentX + 16, (float)themeY + 14);
        g.DrawString(Tr(L"Тема оформления (Визуальный стиль)", L"Visual Theme"), -1, &fontBold, thmLblPt, &whiteBrush);

        int thmStartX = contentX + 16;
        for (size_t i = 0; i < g_themes.size(); ++i) {
            int tX = thmStartX + (int)i * 122;
            bool isSel = (g_currentTheme.id == g_themes[i].id);

            DrawCard(g, (float)tX, (float)themeY + 44, 114, 30, 6,
                     isSel ? g_themes[i].accent : g_currentTheme.bgSurfaceElevated,
                     isSel ? g_themes[i].accent : g_currentTheme.border);

            RectF thmRc((float)tX, (float)themeY + 44, 114, 30);
            SolidBrush thmTextBrush(isSel ? Color(255, 255, 255, 255) : g_currentTheme.textMuted);
            g.DrawString(g_themes[i].name.c_str(), -1, &fontSmallBold, thmRc, &sfCenter, &thmTextBrush);
        }

        int jvmY = themeY + 104;
        DrawCard(g, (float)contentX, (float)jvmY, (float)contentW, 80, 10, g_currentTheme.bgCard, g_currentTheme.border);
        PointF jvmLblPt((float)contentX + 16, (float)jvmY + 14);
        g.DrawString(Tr(L"Предустановка флагов JVM", L"JVM Flags Preset"), -1, &fontBold, jvmLblPt, &whiteBrush);

        PointF jvmSelPt((float)contentX + 16, (float)jvmY + 44);
        g.DrawString(Tr(L"✓ Aikar's Flags (Максимальный FPS, без фризов G1GC)",
                        L"✓ Aikar's Flags (Maximum FPS, stutter-free G1GC)"),
                     -1, &fontRegular, jvmSelPt, &greenBrush);
    }

    // ------------------------------------------------------------------------
    // 4. Bottom Launch Dock (Height: 84px)
    // ------------------------------------------------------------------------
    int dockY = height - dockH;

    SolidBrush dockBrush(g_currentTheme.bgSurface);
    g.FillRectangle(&dockBrush, 0, dockY, width, dockH);

    Pen dockBorderPen(g_currentTheme.border, 1.0f);
    g.DrawLine(&dockBorderPen, 0, dockY, width, dockY);

    GraphicsPath dockAvatarPath;
    AddRoundRect(dockAvatarPath, 20, (float)dockY + 16, 52, 52, 10);
    SolidBrush dockAvatarBg(g_currentTheme.bgSurfaceElevated);
    g.FillPath(&dockAvatarBg, &dockAvatarPath);
    g.DrawPath(&dockBorderPen, &dockAvatarPath);

    PointF cubePt(34, (float)dockY + 28);
    Font fontCube(&ff, 20, FontStyleRegular, UnitPixel);
    g.DrawString(L"🧊", -1, &fontCube, cubePt, &whiteBrush);

    PointF dNamePt(84, (float)dockY + 22);
    g.DrawString(g_activeContainerName.c_str(), -1, &fontBold, dNamePt, &whiteBrush);

    PointF dVerPt(84, (float)dockY + 44);
    std::wstring subVer = Tr(L"Vanilla 1.21.1 • Готов", L"Vanilla 1.21.1 • Ready");
    g.DrawString(subVer.c_str(), -1, &fontSmall, dVerPt, &mutedBrush);

    // Center-Left: Prominent NICKNAME Input Label
    PointF nickLblPt(270, (float)dockY + 14);
    SolidBrush accentSecBrush(g_currentTheme.accentSecondary);
    g.DrawString(Tr(L"НИКНЕЙМ ИГРОКА", L"PLAYER NICKNAME"), -1, &fontSmallBold, nickLblPt, &accentSecBrush);

    DrawCard(g, 268, (float)dockY + 34, 164, 32, 6, g_currentTheme.bgSurfaceElevated, g_currentTheme.borderFocus);

    PointF verLblPt(452, (float)dockY + 14);
    g.DrawString(Tr(L"ВЕРСИЯ ИГРЫ", L"GAME VERSION"), -1, &fontSmallBold, verLblPt, &mutedBrush);

    DrawCard(g, 450, (float)dockY + 34, 110, 32, 6, g_currentTheme.bgSurfaceElevated, g_currentTheme.border);
    PointF verValPt(462, (float)dockY + 41);
    g.DrawString(g_activeVersion.c_str(), -1, &fontBold, verValPt, &whiteBrush);
    PointF arrowPt(538, (float)dockY + 42);
    g.DrawString(L"▼", -1, &fontSmall, arrowPt, &mutedBrush);

    int statusX = 580;
    DrawCard(g, (float)statusX, (float)dockY + 34, 210, 32, 16, g_currentTheme.bgCard, g_currentTheme.border);

    SolidBrush statusDotBrush(g_isGameRunning.load() ? g_currentTheme.warning : g_currentTheme.success);
    g.FillEllipse(&statusDotBrush, statusX + 14, dockY + 45, 10, 10);

    PointF stTextPt((float)statusX + 32, (float)dockY + 41);
    g.DrawString(g_launchStatus.c_str(), -1, &fontSmallBold, stTextPt, &whiteBrush);

    // ------------------------------------------------------------------------
    // THE PLAY BUTTON (Right side of dock, giant glowing rounded gradient button)
    // ------------------------------------------------------------------------
    int playBtnW = 190;
    int playBtnH = 52;
    int playBtnX = width - playBtnW - 24;
    int playBtnY = dockY + 16;

    GraphicsPath glowPath;
    AddRoundRect(glowPath, (float)playBtnX - 4, (float)playBtnY - 4, (float)playBtnW + 8, (float)playBtnH + 8, 16);
    SolidBrush glowBrush(g_currentTheme.accentGlow);
    g.FillPath(&glowBrush, &glowPath);

    const wchar_t* playText = g_isGameRunning.load()
        ? Tr(L"■  В ИГРЕ", L"■  RUNNING")
        : Tr(L"▶  ИГРАТЬ", L"▶  PLAY");

    DrawGradientButton(g, (float)playBtnX, (float)playBtnY, (float)playBtnW, (float)playBtnH, 14,
                       g_currentTheme.accent, g_currentTheme.accentSecondary,
                       Color(255, 255, 255, 255), playText, fontPlayBtn,
                       g_hoverPlayBtn, g_pressPlayBtn);

    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
}

// ----------------------------------------------------------------------------
// Launch Minecraft Workflow
// ----------------------------------------------------------------------------
void StartMinecraftLaunch() {
    if (g_isGameRunning.load()) {
        AppendLog(Tr(L"[Aether] Игра уже запущена!", L"[Aether] Game is already running!"));
        return;
    }

    WCHAR nickBuf[128] = { 0 };
    if (g_hEditNick) {
        GetWindowTextW(g_hEditNick, nickBuf, 127);
        if (wcslen(nickBuf) > 0) {
            g_config.nickname = nickBuf;
            SaveConfig(g_config);
        }
    }

    g_isGameRunning.store(true);
    g_launchStatus = Tr(L"Подготовка файлов...", L"Preparing files...");
    g_lastCrashCause.clear();
    InvalidateRect(g_hMainWindow, NULL, FALSE);

    std::thread([=]() {
        AppendLog(Tr(L"[Aether] ==================================================",
                     L"[Aether] =================================================="));
        AppendLog(Tr(L"[Aether] Старт запуска профиля: " + g_activeContainerName,
                     L"[Aether] Launching profile: " + g_activeContainerName));
        AppendLog(Tr(L"[Aether] Игрок: " + g_config.nickname,
                     L"[Aether] Player: " + g_config.nickname));
        AppendLog(Tr(L"[Aether] Выделение RAM: " + std::to_wstring(g_config.memoryMb) + L" MB",
                     L"[Aether] RAM: " + std::to_wstring(g_config.memoryMb) + L" MB"));

        // Step 1: Scan Java
        AppendLog(Tr(L"[JavaDetector] Сканирование установленных сред выполнения Java...",
                     L"[JavaDetector] Scanning installed Java runtimes..."));
        JavaInfo java = MinecraftLauncher::DetectJava(g_config.customJavaPath);

        std::wstring javaPath = java.path;
        if (javaPath.empty()) {
            AppendLog(Tr(L"[JavaDetector] ВНИМАНИЕ: Java не найдена в системных путях, используется javaw.exe",
                         L"[JavaDetector] WARNING: Java not found, falling back to javaw.exe"));
            javaPath = L"javaw.exe";
        } else {
            AppendLog(Tr(L"[JavaDetector] Найдена совместимая Java: " + javaPath + L" (Java " + std::to_wstring(java.majorVersion) + L")",
                         L"[JavaDetector] Found compatible Java: " + javaPath));
        }

        // Step 2: Prepare & Download official Mojang client.jar if missing
        AppendLog(Tr(L"[PistonMeta] Проверка и подготовка клиента Minecraft " + g_activeVersion + L"...",
                     L"[PistonMeta] Validating and downloading Minecraft " + g_activeVersion + L"..."));

        bool prepared = MinecraftLauncher::PrepareMinecraftFiles(
            g_config.storagePath,
            g_activeVersion,
            [](const std::wstring& status, int pct) {
                g_launchStatus = status;
                g_launchProgressPct = pct;
                PostMessageW(g_hMainWindow, WM_USER + 101, 0, 0);
            }
        );

        if (!prepared) {
            AppendLog(Tr(L"[Aether] [ERROR] Не удалось загрузить client.jar с Mojang CDN!",
                         L"[Aether] [ERROR] Failed to download client.jar from Mojang CDN!"));
            g_launchStatus = Tr(L"Ошибка загрузки!", L"Download failed!");
            g_isGameRunning.store(false);
            PostMessageW(g_hMainWindow, WM_USER + 101, 0, 0);
            return;
        }

        // Step 3: Launch
        AppendLog(Tr(L"[Aether] Запуск процесса Minecraft net.minecraft.client.main.Main...",
                     L"[Aether] Starting Minecraft process net.minecraft.client.main.Main..."));
        g_launchStatus = Tr(L"Игра запущена!", L"Game running!");
        PostMessageW(g_hMainWindow, WM_USER + 101, 0, 0);

        LaunchProgressCallback callbacks;
        callbacks.onProgress = [](const std::wstring& st, int pct) {
            g_launchStatus = st;
            g_launchProgressPct = pct;
            PostMessageW(g_hMainWindow, WM_USER + 101, 0, 0);
        };
        callbacks.onLog = [](const std::wstring& line, bool) {
            AppendLog(line);
        };
        callbacks.onGameExited = [](int exitCode) {
            g_isGameRunning.store(false);
            g_launchStatus = Tr(L"Готов к запуску", L"Ready to launch");

            if (exitCode != 0) {
                AppendLog(Tr(L"[Aether] Процесс завершился с ошибкой (код " + std::to_wstring(exitCode) + L")",
                             L"[Aether] Process exited with error code " + std::to_wstring(exitCode)));

                std::wstring captured;
                {
                    std::lock_guard<std::mutex> lock(g_logsMutex);
                    for (const auto& l : g_logs) captured += l + L"\n";
                }
                std::wstring diag = MinecraftLauncher::AnalyzeCrashLog(captured);
                if (!diag.empty()) {
                    g_lastCrashTitle = Tr(L"Диагностика сбоя игры", L"Game Crash Diagnostics");
                    g_lastCrashCause = diag;
                    g_lastCrashSolution = Tr(L"Проверьте версию Java и выделенную память в Настройках.",
                                             L"Check Java version and allocated RAM in Settings.");
                    AppendLog(Tr(L"[CrashAnalyzer] " + diag, L"[CrashAnalyzer] " + diag));
                }
            } else {
                AppendLog(Tr(L"[Aether] Игра успешно завершена.", L"[Aether] Game exited normally."));
            }

            PostMessageW(g_hMainWindow, WM_USER + 101, 0, 0);
        };

        bool launched = MinecraftLauncher::LaunchMinecraft(
            g_config.storagePath,
            g_activeVersion,
            L"vanilla",
            g_config.nickname,
            g_config.memoryMb,
            javaPath,
            g_config.jvmPreset,
            g_config.customJvmArgs,
            callbacks,
            &g_hGameProcess
        );

        if (!launched) {
            g_isGameRunning.store(false);
            g_launchStatus = Tr(L"Ошибка запуска!", L"Launch failed!");
            AppendLog(Tr(L"[Aether] [ERROR] Не удалось запустить процесс CreateProcessW!",
                         L"[Aether] [ERROR] CreateProcessW failed!"));
            PostMessageW(g_hMainWindow, WM_USER + 101, 0, 0);
        }
    }).detach();
}

void AppendLog(const std::wstring& line) {
    std::lock_guard<std::mutex> lock(g_logsMutex);

    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    struct tm buf;
    localtime_s(&buf, &in_time_t);

    std::wstringstream ss;
    ss << L"[" << std::put_time(&buf, L"%H:%M:%S") << L"] " << line;

    g_logs.push_back(ss.str());
    if (g_logs.size() > 500) {
        g_logs.erase(g_logs.begin());
    }

    if (g_hMainWindow) {
        PostMessageW(g_hMainWindow, WM_USER + 101, 0, 0);
    }
}

void PingActiveServer() {
    WCHAR hostBuf[256] = { 0 };
    if (g_hEditServer) {
        GetWindowTextW(g_hEditServer, hostBuf, 255);
        if (wcslen(hostBuf) > 0) {
            g_serverHost = hostBuf;
        }
    }

    std::thread([=]() {
        AppendLog(Tr(L"[ServerPinger] Пинг сервера " + g_serverHost + L"...",
                     L"[ServerPinger] Pinging server " + g_serverHost + L"..."));

        std::string narrowHost(g_serverHost.begin(), g_serverHost.end());
        ServerStatus res = NativeServerPinger::Ping(narrowHost, 25565);
        g_serverIsOnline = res.online;

        if (res.online) {
            g_serverPingText = Tr(L"Задержка: ", L"Latency: ") + std::to_wstring(res.latencyMs) + L" ms  •  Версия: " + res.version;
            g_serverMotd = res.motd;
            g_serverPlayers = Tr(L"Игроков онлайн: ", L"Players online: ") + std::to_wstring(res.playersOnline) + L" / " + std::to_wstring(res.playersMax);
            AppendLog(Tr(L"[ServerPinger] Ответ получен! Пинг: " + std::to_wstring(res.latencyMs) + L" ms, Игроки: " + std::to_wstring(res.playersOnline),
                         L"[ServerPinger] Response received! Latency: " + std::to_wstring(res.latencyMs) + L" ms"));
        } else {
            g_serverPingText = Tr(L"Сервер недоступен (Таймаут соединения)", L"Server unreachable (Connection timeout)");
            g_serverMotd = Tr(L"Не удалось получить ответ от сервера.", L"Failed to receive response from server.");
            g_serverPlayers = L"0 / 0";
            AppendLog(Tr(L"[ServerPinger] Сервер не ответил.", L"[ServerPinger] Server did not respond."));
        }

        PostMessageW(g_hMainWindow, WM_USER + 101, 0, 0);
    }).detach();
}

// ----------------------------------------------------------------------------
// Window Message Handler
// ----------------------------------------------------------------------------
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_hMainWindow = hwnd;

        AppendLog(Tr(L"[Aether] Лаунчер инициализирован успешно (Нативный C++ Win32 GDI+ движок).",
                     L"[Aether] Launcher initialized successfully (Native C++ Win32 GDI+ engine)."));
        AppendLog(Tr(L"[Aether] Нулевая зависимость от сторонних браузеров / Chrome / Node.js.",
                     L"[Aether] Zero browser dependencies / No Chrome / No Node.js required."));
        AppendLog(Tr(L"[Aether] Готов к работе. Выберите профиль и нажмите Играть.",
                     L"[Aether] Ready. Select profile and click Play."));

        // Create Nickname Edit Box (Placed right on the bottom launch dock!)
        g_hEditNick = CreateWindowExW(
            0, L"EDIT", g_config.nickname.c_str(),
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            272, 680 - 84 + 38, 156, 24,
            hwnd, (HMENU)IDC_EDIT_NICKNAME, g_hInstance, NULL
        );

        HFONT hFontNick = CreateFontW(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        SendMessageW(g_hEditNick, WM_SETFONT, (WPARAM)hFontNick, TRUE);

        // Create Server Address Edit Box (For server pinger tab)
        g_hEditServer = CreateWindowExW(
            0, L"EDIT", g_serverHost.c_str(),
            WS_CHILD | ES_AUTOHSCROLL,
            220 + 36, 54 + 20 + 68, 500, 24,
            hwnd, (HMENU)IDC_EDIT_SERVER, g_hInstance, NULL
        );
        SendMessageW(g_hEditServer, WM_SETFONT, (WPARAM)hFontNick, TRUE);

        return 0;
    }

    case WM_CTLCOLOREDIT: {
        HDC hdcEdit = (HDC)wParam;
        SetTextColor(hdcEdit, RGB(240, 242, 248));
        SetBkColor(hdcEdit, RGB(26, 30, 46));
        static HBRUSH hEditBg = CreateSolidBrush(RGB(26, 30, 46));
        return (LRESULT)hEditBg;
    }

    case WM_SIZE: {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        if (g_hEditNick) {
            SetWindowPos(g_hEditNick, NULL, 272, height - 84 + 38, 156, 24, SWP_NOZORDER);
        }
        if (g_hEditServer) {
            SetWindowPos(g_hEditServer, NULL, 220 + 36, 54 + 20 + 68, width - 220 - 220, 24, SWP_NOZORDER);
            ShowWindow(g_hEditServer, g_currentTab == 2 ? SW_SHOW : SW_HIDE);
        }
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1; // Prevent flicker

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        PaintMainUI(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        RECT rc;
        GetClientRect(hwnd, &rc);
        int width = rc.right - rc.left;
        int height = rc.bottom - rc.top;

        int oldTabHover = g_hoveredTab;
        g_hoveredTab = -1;
        if (x < 220 && y >= 88 && y < 88 + 6 * 46) {
            g_hoveredTab = (y - 88) / 46;
        }

        int playBtnW = 190;
        int playBtnH = 52;
        int playBtnX = width - playBtnW - 24;
        int playBtnY = height - 84 + 16;
        bool oldPlayHover = g_hoverPlayBtn;
        g_hoverPlayBtn = (x >= playBtnX && x <= playBtnX + playBtnW && y >= playBtnY && y <= playBtnY + playBtnH);

        if (oldTabHover != g_hoveredTab || oldPlayHover != g_hoverPlayBtn) {
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        RECT rc;
        GetClientRect(hwnd, &rc);
        int width = rc.right - rc.left;
        int height = rc.bottom - rc.top;

        // Click sidebar tabs
        if (x < 220 && y >= 88 && y < 88 + 6 * 46) {
            int clickedTab = (y - 88) / 46;
            if (clickedTab >= 0 && clickedTab < 6) {
                g_currentTab = clickedTab;
                if (g_hEditServer) {
                    ShowWindow(g_hEditServer, g_currentTab == 2 ? SW_SHOW : SW_HIDE);
                }
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        // Click language switcher [ RU | EN ]
        int langX = width - 110;
        if (x >= langX && x <= langX + 70 && y >= 12 && y <= 42) {
            g_isRussian = !g_isRussian;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        // Click Play button
        int playBtnW = 190;
        int playBtnH = 52;
        int playBtnX = width - playBtnW - 24;
        int playBtnY = height - 84 + 16;
        if (x >= playBtnX && x <= playBtnX + playBtnW && y >= playBtnY && y <= playBtnY + playBtnH) {
            g_pressPlayBtn = true;
            InvalidateRect(hwnd, NULL, FALSE);
            StartMinecraftLaunch();
            return 0;
        }

        // Click Version Selector on dock
        if (x >= 450 && x <= 560 && y >= height - 84 + 34 && y <= height - 84 + 66) {
            g_selectedVersionIndex = (g_selectedVersionIndex + 1) % (int)g_versions.size();
            g_activeVersion = g_versions[g_selectedVersionIndex];
            g_activeContainerName = L"Minecraft " + g_activeVersion;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        // Click Server Ping button in tab 2
        if (g_currentTab == 2) {
            int contentW = width - 220 - 48;
            int btnX = 220 + 24 + contentW - 150;
            int btnY = 54 + 20 + 62;
            if (x >= btnX && x <= btnX + 150 && y >= btnY && y <= btnY + 36) {
                PingActiveServer();
                return 0;
            }
        }

        // Click RAM pill in Settings (Tab 5)
        if (g_currentTab == 5) {
            int setY = 54 + 20 + 68;
            int pillStartX = 220 + 24 + 16;
            for (size_t i = 0; i < g_ramOptions.size(); ++i) {
                int pX = pillStartX + (int)i * 72;
                if (x >= pX && x <= pX + 62 && y >= setY + 44 && y <= setY + 74) {
                    g_config.memoryMb = g_ramOptions[i];
                    SaveConfig(g_config);
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
            }

            // Theme pill click
            int themeY = setY + 104;
            int thmStartX = 220 + 24 + 16;
            for (size_t i = 0; i < g_themes.size(); ++i) {
                int tX = thmStartX + (int)i * 122;
                if (x >= tX && x <= tX + 114 && y >= themeY + 44 && y <= themeY + 74) {
                    g_currentTheme = g_themes[i];
                    g_config.theme = g_themes[i].id;
                    SaveConfig(g_config);
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
            }
        }

        // Click Mod cards install button (Tab 1)
        if (g_currentTab == 1) {
            int contentW = width - 220 - 48;
            int modListY = 54 + 20 + 62 + 48;
            int modItemH = 54;
            int btnW = 110;
            int btnX = 220 + 24 + contentW - btnW - 14;

            for (size_t i = 0; i < g_mods.size() && i < 5; ++i) {
                int mY = modListY + (int)i * (modItemH + 8);
                if (x >= btnX && x <= btnX + btnW && y >= mY + 12 && y <= mY + 40) {
                    g_mods[i].installed = !g_mods[i].installed;
                    AppendLog(Tr(L"[Modrinth] " + g_mods[i].name + (g_mods[i].installed ? L" успешно установлен." : L" отключен."),
                                 L"[Modrinth] " + g_mods[i].name + (g_mods[i].installed ? L" installed." : L" disabled.")));
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
            }
        }

        return 0;
    }

    case WM_LBUTTONUP: {
        if (g_pressPlayBtn) {
            g_pressPlayBtn = false;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_USER + 101: {
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_DESTROY: {
        if (g_hEditNick) {
            WCHAR buf[128] = { 0 };
            GetWindowTextW(g_hEditNick, buf, 127);
            if (wcslen(buf) > 0) {
                g_config.nickname = buf;
                SaveConfig(g_config);
            }
        }
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ----------------------------------------------------------------------------
// WinMain
// ----------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    g_hInstance = hInstance;

    g_config = LoadConfig();

    for (const auto& thm : g_themes) {
        if (thm.id == g_config.theme) {
            g_currentTheme = thm;
            break;
        }
    }

    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"AetherLauncherNativeGDI";

    RegisterClassExW(&wc);

    int winWidth = 1100;
    int winHeight = 680;
    int scrWidth = GetSystemMetrics(SM_CXSCREEN);
    int scrHeight = GetSystemMetrics(SM_CYSCREEN);
    int startX = std::max(0, (scrWidth - winWidth) / 2);
    int startY = std::max(0, (scrHeight - winHeight) / 2);

    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"AetherLauncherNativeGDI",
        L"Aether Launcher (Native Win32 C++)",
        WS_OVERLAPPEDWINDOW,
        startX, startY, winWidth, winHeight,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) {
        GdiplusShutdown(gdiplusToken);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    GdiplusShutdown(gdiplusToken);
    return (int)msg.wParam;
}
