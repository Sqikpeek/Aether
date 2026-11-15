// ============================================================================
// Aether Launcher - Native Modern C++ Win32 GUI Engine (GDI+)
// Pure Native Desktop Application with Zero Browser / Chrome / Node Dependencies
// Features: Vector GDI+ Anti-Aliased Icons, Double-Buffering (60 FPS),
// Single Default Container & Full Selection, Real-Time Mod Search via App Tab,
// Interactive RAM Slider with Custom Numeric Input, and In-Dock Nickname.
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
// Container Profiles - Only 1 Default Container by User Request
// ----------------------------------------------------------------------------
struct ContainerProfile {
    std::wstring id;
    std::wstring name;
    std::wstring version;
    std::wstring loader;
    int memoryMb;
};

static std::vector<ContainerProfile> g_containers = {
    { L"c-default", L"Minecraft 1.21.1 Vanilla", L"1.21.1", L"vanilla", 4096 }
};
static int g_activeContainerIndex = 0;

// ----------------------------------------------------------------------------
// Global UI State & Controls
// ----------------------------------------------------------------------------
static HINSTANCE g_hInstance = NULL;
static HWND g_hMainWindow = NULL;
static HWND g_hEditNick = NULL;
static HWND g_hEditModSearch = NULL;
static HWND g_hEditRam = NULL;

#define IDC_EDIT_NICKNAME  1001
#define IDC_EDIT_MODSEARCH 1002
#define IDC_EDIT_RAM       1003

static LauncherConfig g_config;
// 0: Containers, 1: Mods, 2: Logs & Crashes, 3: Backups, 4: Settings
static int g_currentTab = 0;
static int g_hoveredTab = -1;
static bool g_hoverPlayBtn = false;
static bool g_pressPlayBtn = false;
static bool g_isRussian = true;
static bool g_isDraggingSlider = false;

// Active container & launch status
static std::wstring g_activeContainerName = L"Minecraft 1.21.1 Vanilla";
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

// Mod Items & Search Filter
struct ModItem {
    std::wstring name;
    std::wstring description;
    std::wstring downloads;
    std::wstring category;
    bool installed;
};

static std::vector<ModItem> g_allMods = {
    { L"Sodium", L"Современный движок рендеринга для Fabric, колоссально повышающий FPS и плавность.", L"32.4M", L"Оптимизация", true },
    { L"Iris Shaders", L"Современный загрузчик шейдер-паков с поддержкой PBR и совместимостью с Sodium.", L"24.1M", L"Шейдеры", true },
    { L"Lithium", L"Глубокая оптимизация тика сервера, физики мобов и чанков без изменения механик.", L"19.8M", L"Оптимизация", false },
    { L"Fabric API", L"Основная библиотека хуков и событий, необходимая для большинства модов Fabric.", L"45.2M", L"Библиотека", true },
    { L"AppleSkin", L"Удобное отображение насыщения и истощения полоски голода над хотбаром.", L"28.6M", L"Интерфейс", false },
    { L"JourneyMap", L"Красивая карта реального времени с радаром мобов и путевыми точками (waypoint).", L"38.1M", L"Карта", false },
    { L"Just Enough Items (JEI)", L"Просмотр рецептов крафта, плавки и использования предметов прямо в инвентаре.", L"120.5M", L"Интерфейс", false },
    { L"Create", L"Грандиозный мод на механику, шестеренки, конвейеры, поезда и автоматизацию.", L"54.8M", L"Технологии", false },
    { L"FerriteCore", L"Экстремальная оптимизация потребления оперативной памяти Java кучи.", L"22.3M", L"Оптимизация", false },
    { L"Mod Menu", L"Удобный список установленных модов с кнопками настроек прямо в меню игры.", L"41.2M", L"Интерфейс", true }
};

static std::vector<ModItem> g_filteredMods = g_allMods;
static std::wstring g_modSearchQuery = L"";

// Available versions
static std::vector<std::wstring> g_versions = {
    L"1.21.1", L"1.20.4", L"1.20.1", L"1.19.4", L"1.18.2", L"1.16.5", L"1.12.2"
};
static int g_selectedVersionIndex = 0;

// Quick RAM presets (MB)
static std::vector<int> g_ramPresets = { 2048, 4096, 6144, 8192, 12288, 16384 };

// Forward declarations
void AppendLog(const std::wstring& line);
void StartMinecraftLaunch();
void FilterMods();
void SetActiveContainer(int index);

// Translation Helper
inline const wchar_t* Tr(const wchar_t* ru, const wchar_t* en) {
    return g_isRussian ? ru : en;
}
inline std::wstring Tr(const std::wstring& ru, const std::wstring& en) {
    return g_isRussian ? ru : en;
}

inline std::wstring ToLowerW(const std::wstring& s) {
    std::wstring r = s;
    for (auto& c : r) c = towlower(c);
    return r;
}

// ----------------------------------------------------------------------------
// Crisp Vector Icons (100% Native GDI+ Paths - Zero Missing Glyphs / Tofu)
// ----------------------------------------------------------------------------
static void DrawIconContainers(Graphics& g, float x, float y, float size, Color col) {
    Pen pen(col, 1.8f);
    float half = size * 0.45f;
    float gap = size * 0.1f;
    g.DrawRectangle(&pen, x, y, half, half);
    g.DrawRectangle(&pen, x + half + gap, y, half, half);
    g.DrawRectangle(&pen, x, y + half + gap, half, half);
    g.DrawRectangle(&pen, x + half + gap, y + half + gap, half, half);
}

static void DrawIconMods(Graphics& g, float x, float y, float size, Color col) {
    Pen pen(col, 1.8f);
    float s = size;
    PointF pts[8] = {
        PointF(x + s * 0.2f, y),
        PointF(x + s * 0.8f, y),
        PointF(x + s, y + s * 0.35f),
        PointF(x + s * 0.85f, y + s * 0.5f),
        PointF(x + s, y + s * 0.65f),
        PointF(x + s * 0.8f, y + s),
        PointF(x + s * 0.2f, y + s),
        PointF(x, y + s * 0.5f)
    };
    g.DrawPolygon(&pen, pts, 8);
}

static void DrawIconLogs(Graphics& g, float x, float y, float size, Color col) {
    Pen pen(col, 2.0f);
    g.DrawLine(&pen, x, y + 2, x + size * 0.45f, y + size * 0.45f);
    g.DrawLine(&pen, x + size * 0.45f, y + size * 0.45f, x, y + size * 0.85f);
    g.DrawLine(&pen, x + size * 0.55f, y + size * 0.85f, x + size, y + size * 0.85f);
}

static void DrawIconBackups(Graphics& g, float x, float y, float size, Color col) {
    Pen pen(col, 1.8f);
    float s = size;
    PointF pts[5] = {
        PointF(x, y),
        PointF(x + s * 0.75f, y),
        PointF(x + s, y + s * 0.25f),
        PointF(x + s, y + s),
        PointF(x, y + s)
    };
    g.DrawPolygon(&pen, pts, 5);
    g.DrawRectangle(&pen, x + s * 0.25f, y, s * 0.4f, s * 0.35f);
}

static void DrawIconSettings(Graphics& g, float x, float y, float size, Color col) {
    Pen pen(col, 1.8f);
    float s = size;
    g.DrawLine(&pen, x, y + s * 0.25f, x + s, y + s * 0.25f);
    SolidBrush brush(col);
    g.FillEllipse(&brush, x + s * 0.3f - 2.5f, y + s * 0.25f - 2.5f, 5.0f, 5.0f);

    g.DrawLine(&pen, x, y + s * 0.75f, x + s, y + s * 0.75f);
    g.FillEllipse(&brush, x + s * 0.7f - 2.5f, y + s * 0.75f - 2.5f, 5.0f, 5.0f);
}

static void DrawIconSearch(Graphics& g, float x, float y, float size, Color col) {
    Pen pen(col, 1.8f);
    float r = size * 0.35f;
    g.DrawEllipse(&pen, x, y, r * 2.0f, r * 2.0f);
    g.DrawLine(&pen, x + r * 1.6f, y + r * 1.6f, x + size, y + size);
}

static void DrawIconCube(Graphics& g, float cx, float cy, float r, Color colTop, Color colLeft, Color colRight) {
    PointF topPts[4] = {
        PointF(cx, cy - r),
        PointF(cx + r * 0.866f, cy - r * 0.5f),
        PointF(cx, cy),
        PointF(cx - r * 0.866f, cy - r * 0.5f)
    };
    SolidBrush bTop(colTop);
    g.FillPolygon(&bTop, topPts, 4);

    PointF leftPts[4] = {
        PointF(cx - r * 0.866f, cy - r * 0.5f),
        PointF(cx, cy),
        PointF(cx, cy + r),
        PointF(cx - r * 0.866f, cy + r * 0.5f)
    };
    SolidBrush bLeft(colLeft);
    g.FillPolygon(&bLeft, leftPts, 4);

    PointF rightPts[4] = {
        PointF(cx, cy),
        PointF(cx + r * 0.866f, cy - r * 0.5f),
        PointF(cx + r * 0.866f, cy + r * 0.5f),
        PointF(cx, cy + r)
    };
    SolidBrush bRight(colRight);
    g.FillPolygon(&bRight, rightPts, 4);
}

static void DrawIconCheck(Graphics& g, float x, float y, float size, Color col) {
    Pen pen(col, 2.0f);
    g.DrawLine(&pen, x, y + size * 0.5f, x + size * 0.35f, y + size);
    g.DrawLine(&pen, x + size * 0.35f, y + size, x + size, y);
}

static void DrawIconGamepad(Graphics& g, float x, float y, float w, float h, Color col) {
    Pen pen(col, 1.8f);
    g.DrawEllipse(&pen, x, y + 2, w * 0.35f, h * 0.7f);
    g.DrawEllipse(&pen, x + w * 0.65f, y + 2, w * 0.35f, h * 0.7f);
    g.DrawLine(&pen, x + w * 0.18f, y + 2, x + w * 0.82f, y + 2);
    g.DrawLine(&pen, x + w * 0.18f, y + h * 0.7f, x + w * 0.82f, y + h * 0.7f);
}

static void DrawIconWarn(Graphics& g, float x, float y, float size, Color col) {
    Pen pen(col, 1.8f);
    PointF pts[3] = {
        PointF(x + size * 0.5f, y),
        PointF(x + size, y + size),
        PointF(x, y + size)
    };
    g.DrawPolygon(&pen, pts, 3);
    g.DrawLine(&pen, x + size * 0.5f, y + size * 0.35f, x + size * 0.5f, y + size * 0.65f);
    SolidBrush dot(col);
    g.FillEllipse(&dot, x + size * 0.5f - 1.5f, y + size * 0.78f, 3.0f, 3.0f);
}

// ----------------------------------------------------------------------------
// Basic GDI+ Shapes
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
// Container Selection Helper
// ----------------------------------------------------------------------------
void SetActiveContainer(int index) {
    if (index >= 0 && index < (int)g_containers.size()) {
        g_activeContainerIndex = index;
        g_activeContainerName = g_containers[index].name;
        g_activeVersion = g_containers[index].version;
        g_config.version = g_containers[index].version;
        g_config.loader = g_containers[index].loader;
        g_config.memoryMb = g_containers[index].memoryMb;
        SaveConfig(g_config);

        if (g_hEditRam) {
            SetWindowTextW(g_hEditRam, std::to_wstring(g_config.memoryMb).c_str());
        }

        AppendLog(Tr(L"[Aether] Активный контейнер изменен на: " + g_activeContainerName,
                     L"[Aether] Active container set to: " + g_activeContainerName));

        if (g_hMainWindow) {
            InvalidateRect(g_hMainWindow, NULL, FALSE);
        }
    }
}

// ----------------------------------------------------------------------------
// Mod Filter Logic
// ----------------------------------------------------------------------------
void FilterMods() {
    if (g_modSearchQuery.empty()) {
        g_filteredMods = g_allMods;
        return;
    }
    std::wstring q = ToLowerW(g_modSearchQuery);
    g_filteredMods.clear();
    for (const auto& m : g_allMods) {
        if (ToLowerW(m.name).find(q) != std::wstring::npos ||
            ToLowerW(m.description).find(q) != std::wstring::npos ||
            ToLowerW(m.category).find(q) != std::wstring::npos) {
            g_filteredMods.push_back(m);
        }
    }
}

// ----------------------------------------------------------------------------
// Main UI Painting Routine
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
    // 1. Sidebar (Width: 220px) - Servers Tab Removed by Request!
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

    GraphicsPath badgePath;
    AddRoundRect(badgePath, 68, 42, 64, 14, 4);
    SolidBrush badgeBg(Color(40, g_currentTheme.accent.GetR(), g_currentTheme.accent.GetG(), g_currentTheme.accent.GetB()));
    g.FillPath(&badgeBg, &badgePath);
    SolidBrush accentBrush(g_currentTheme.accentSecondary);
    RectF badgeTextRc(68, 42, 64, 14);
    Font fontPill(&ff, 9, FontStyleBold, UnitPixel);
    g.DrawString(L"LAUNCHER", -1, &fontPill, badgeTextRc, &sfCenter, &accentBrush);

    g.DrawLine(&borderPen, 20, 72, sidebarW - 20, 72);

    // Sidebar Navigation Items (Total 5 Tabs: Servers Tab Removed!)
    struct NavItem {
        int iconType;
        const wchar_t* titleRu;
        const wchar_t* titleEn;
    };
    static NavItem navItems[] = {
        { 0, L"Контейнеры", L"Containers" },
        { 1, L"Моды (Modrinth)", L"Mods (Modrinth)" },
        { 2, L"Логи и краши", L"Logs & Crashes" },
        { 3, L"Резервные копии", L"Backups" },
        { 4, L"Настройки", L"Settings" }
    };

    int navStartY = 88;
    int navItemH = 42;
    for (int i = 0; i < 5; ++i) {
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

        Color iconCol = isActive ? g_currentTheme.textMain : g_currentTheme.textMuted;
        float iconX = 24.0f;
        float iconY = (float)itemY + 13.0f;

        switch (navItems[i].iconType) {
        case 0: DrawIconContainers(g, iconX, iconY, 16.0f, iconCol); break;
        case 1: DrawIconMods(g, iconX, iconY, 16.0f, iconCol); break;
        case 2: DrawIconLogs(g, iconX, iconY, 16.0f, iconCol); break;
        case 3: DrawIconBackups(g, iconX, iconY, 16.0f, iconCol); break;
        case 4: DrawIconSettings(g, iconX, iconY, 16.0f, iconCol); break;
        }

        PointF textPt(50, (float)itemY + 11);
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

    // Clickable Active Container Selector Pill
    DrawCard(g, (float)sidebarW + 20, 11, 280, 32, 8, g_currentTheme.bgSurfaceElevated, g_currentTheme.borderFocus);
    DrawIconGamepad(g, (float)sidebarW + 32, 18, 18, 16, g_currentTheme.accentSecondary);
    PointF activeNamePt((float)sidebarW + 60, 17);
    g.DrawString(g_activeContainerName.c_str(), -1, &fontBold, activeNamePt, &whiteBrush);
    PointF arrowPillPt((float)sidebarW + 280, 18);
    g.DrawString(L"▼", -1, &fontSmall, arrowPillPt, &mutedBrush);

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
        // --- Containers Tab (Only 1 Default Container with Selection) ---
        PointF titlePt((float)contentX, (float)contentY);
        g.DrawString(Tr(L"Игровые контейнеры", L"Game Containers"), -1, &fontTitle, titlePt, &whiteBrush);

        PointF subPt((float)contentX, (float)contentY + 28);
        g.DrawString(Tr(L"Изолированные профили для версий Minecraft, сборок и модов",
                        L"Isolated profiles for Minecraft versions, modpacks and loaders"),
                     -1, &fontSubtitle, subPt, &mutedBrush);

        // "+ Создать контейнер" button on the right
        int addBtnW = 180;
        int addBtnX = contentX + contentW - addBtnW;
        DrawGradientButton(g, (float)addBtnX, (float)contentY, (float)addBtnW, 34, 8,
                           g_currentTheme.accent, g_currentTheme.accentSecondary,
                           Color(255, 255, 255, 255), Tr(L"+ Создать контейнер", L"+ Create Container"), fontSmallBold, false, false);

        int cardY = contentY + 68;
        int cardW = contentW;
        int cardH = 120;

        for (size_t i = 0; i < g_containers.size(); ++i) {
            int cY = cardY + (int)i * (cardH + 16);
            bool isActive = ((int)i == g_activeContainerIndex);

            DrawCard(g, (float)contentX, (float)cY, (float)cardW, (float)cardH, 12,
                     g_currentTheme.bgCard, isActive ? g_currentTheme.borderFocus : g_currentTheme.border);

            // 3D Isometric Cube Icon
            DrawIconCube(g, (float)contentX + 36, (float)cY + 36, 18.0f,
                         g_currentTheme.accentSecondary, g_currentTheme.accent, Color(255, 100, 50, 160));

            PointF cTitlePt((float)contentX + 70, (float)cY + 16);
            g.DrawString(g_containers[i].name.c_str(), -1, &fontHeader, cTitlePt, &whiteBrush);

            // Loader & Version Badges
            DrawCard(g, (float)contentX + 70, (float)cY + 42, 65, 20, 4, Color(40, 16, 185, 129), Color(100, 16, 185, 129));
            RectF b1Rc((float)contentX + 70, (float)cY + 42, 65, 20);
            g.DrawString(g_containers[i].loader.c_str(), -1, &fontSmallBold, b1Rc, &sfCenter, &greenBrush);

            DrawCard(g, (float)contentX + 142, (float)cY + 42, 55, 20, 4, g_currentTheme.bgSurfaceElevated, g_currentTheme.border);
            RectF b2Rc((float)contentX + 142, (float)cY + 42, 55, 20);
            g.DrawString(g_containers[i].version.c_str(), -1, &fontSmall, b2Rc, &sfCenter, &mutedBrush);

            PointF cStatsPt((float)contentX + 70, (float)cY + 72);
            std::wstring stats = Tr(L"Выделенная память: ", L"Memory: ") + std::to_wstring(g_containers[i].memoryMb / 1024) +
                                 L" GB  •  " + Tr(L"Общие миры: Вкл  •  Статус: Готов", L"Shared Saves: On  •  Status: Ready");
            g.DrawString(stats.c_str(), -1, &fontSmall, cStatsPt, &mutedBrush);

            // Right action pill: Active indicator or Select button
            int btnW = 150;
            int btnX = contentX + cardW - btnW - 20;
            int btnY = cY + (cardH - 32) / 2;

            if (isActive) {
                DrawCard(g, (float)btnX, (float)btnY, (float)btnW, 32, 6, g_currentTheme.accent, g_currentTheme.accent);
                DrawIconCheck(g, (float)btnX + 18, (float)btnY + 10, 12.0f, Color(255, 255, 255, 255));
                PointF selTextPt((float)btnX + 38, (float)btnY + 7);
                g.DrawString(Tr(L"Активен", L"Active"), -1, &fontSmallBold, selTextPt, &whiteBrush);
            } else {
                DrawCard(g, (float)btnX, (float)btnY, (float)btnW, 32, 6, g_currentTheme.bgSurfaceElevated, g_currentTheme.border);
                RectF selRc((float)btnX, (float)btnY, (float)btnW, 32);
                g.DrawString(Tr(L"Выбрать контейнер", L"Select Container"), -1, &fontSmallBold, selRc, &sfCenter, &mutedBrush);
            }
        }
    }
    else if (g_currentTab == 1) {
        // --- Modrinth Mods Tab (Fixed Real-Time Search via Edit Control) ---
        PointF titlePt((float)contentX, (float)contentY);
        g.DrawString(Tr(L"Каталог Modrinth v2", L"Modrinth v2 Catalog"), -1, &fontTitle, titlePt, &whiteBrush);

        PointF subPt((float)contentX, (float)contentY + 28);
        g.DrawString(Tr(L"Поиск и установка модов, датапаков и шейдеров прямо в лаунчере",
                        L"Search and install mods, datapacks and shaders directly in launcher"),
                     -1, &fontSubtitle, subPt, &mutedBrush);

        int searchY = contentY + 62;
        // Search bar card background (The real Win32 EDIT control g_hEditModSearch sits inside!)
        DrawCard(g, (float)contentX, (float)searchY, (float)contentW - 100, 36, 8, g_currentTheme.bgSurfaceElevated, g_currentTheme.borderFocus);
        DrawIconSearch(g, (float)contentX + 12, (float)searchY + 10, 16.0f, g_currentTheme.accentSecondary);

        // "Найти" Button
        DrawGradientButton(g, (float)contentX + contentW - 90, (float)searchY, 90, 36, 8,
                           g_currentTheme.accent, g_currentTheme.accentSecondary,
                           Color(255, 255, 255, 255), Tr(L"Найти", L"Search"), fontBold, false, false);

        int modListY = searchY + 48;
        int modItemH = 54;

        if (g_filteredMods.empty()) {
            PointF emptyPt((float)contentX + 16, (float)modListY + 20);
            g.DrawString(Tr(L"Моды не найдены по данному запросу.", L"No mods found matching your query."), -1, &fontRegular, emptyPt, &mutedBrush);
        } else {
            for (size_t i = 0; i < g_filteredMods.size() && i < 6; ++i) {
                int mY = modListY + (int)i * (modItemH + 8);
                DrawCard(g, (float)contentX, (float)mY, (float)contentW, (float)modItemH, 8, g_currentTheme.bgCard, g_currentTheme.border);

                PointF mNamePt((float)contentX + 16, (float)mY + 10);
                g.DrawString(g_filteredMods[i].name.c_str(), -1, &fontBold, mNamePt, &whiteBrush);

                DrawCard(g, (float)contentX + 180, (float)mY + 10, 85, 18, 4, g_currentTheme.bgSurfaceElevated, g_currentTheme.border);
                RectF tagRc((float)contentX + 180, (float)mY + 10, 85, 18);
                g.DrawString(g_filteredMods[i].category.c_str(), -1, &fontSmall, tagRc, &sfCenter, &mutedBrush);

                PointF mDescPt((float)contentX + 16, (float)mY + 30);
                g.DrawString(g_filteredMods[i].description.c_str(), -1, &fontSmall, mDescPt, &mutedBrush);

                int btnW = 120;
                int btnX = contentX + contentW - btnW - 14;
                if (g_filteredMods[i].installed) {
                    DrawCard(g, (float)btnX, (float)mY + 12, (float)btnW, 28, 6, Color(40, 16, 185, 129), g_currentTheme.success);
                    DrawIconCheck(g, (float)btnX + 12, (float)mY + 20, 11.0f, g_currentTheme.success);
                    PointF instTextPt((float)btnX + 28, (float)mY + 17);
                    SolidBrush greenText(g_currentTheme.success);
                    g.DrawString(Tr(L"Установлен", L"Installed"), -1, &fontSmallBold, instTextPt, &greenText);
                } else {
                    DrawGradientButton(g, (float)btnX, (float)mY + 12, (float)btnW, 28, 6,
                                       g_currentTheme.accent, g_currentTheme.accentSecondary,
                                       Color(255, 255, 255, 255), Tr(L"+ Установить", L"+ Install"), fontSmallBold, false, false);
                }
            }
        }
    }
    else if (g_currentTab == 2) {
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
            DrawIconWarn(g, (float)contentX + 16, (float)logY + 14, 18.0f, g_currentTheme.warning);

            PointF diagTitlePt((float)contentX + 44, (float)logY + 12);
            g.DrawString(g_lastCrashTitle.c_str(), -1, &fontBold, diagTitlePt, &warnBrush);

            PointF diagCausePt((float)contentX + 44, (float)logY + 32);
            g.DrawString(g_lastCrashCause.c_str(), -1, &fontSmall, diagCausePt, &whiteBrush);

            PointF diagSolPt((float)contentX + 44, (float)logY + 50);
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
    else if (g_currentTab == 3) {
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
        DrawIconBackups(g, (float)contentX + 16, (float)listY + 22, 18.0f, g_currentTheme.accentSecondary);

        PointF bTitlePt((float)contentX + 44, (float)listY + 14);
        g.DrawString(Tr(L"Vanilla 1.21.1 - Снимок перед установкой модов", L"Vanilla 1.21.1 - Snapshot before mod test"), -1, &fontBold, bTitlePt, &whiteBrush);

        PointF bTimePt((float)contentX + 44, (float)listY + 36);
        g.DrawString(Tr(L"Сегодня, 14:20  •  Размер: 42.1 МБ", L"Today, 14:20  •  Size: 42.1 MB"), -1, &fontSmall, bTimePt, &mutedBrush);

        DrawCard(g, (float)contentX + contentW - 140, (float)listY + 18, 120, 28, 6, g_currentTheme.bgSurfaceElevated, g_currentTheme.border);
        RectF rstRc((float)contentX + contentW - 140, (float)listY + 18, 120, 28);
        g.DrawString(Tr(L"Восстановить", L"Restore"), -1, &fontSmallBold, rstRc, &sfCenter, &whiteBrush);
    }
    else if (g_currentTab == 4) {
        // --- Settings Tab: RAM Draggable Slider & Custom Numeric Input ---
        PointF titlePt((float)contentX, (float)contentY);
        g.DrawString(Tr(L"Настройки лаунчера", L"Launcher Settings"), -1, &fontTitle, titlePt, &whiteBrush);

        PointF subPt((float)contentX, (float)contentY + 28);
        g.DrawString(Tr(L"Память, автоподбор Java, оптимизация и визуальные темы",
                        L"Memory allocation, Java scanner, optimization and themes"),
                     -1, &fontSubtitle, subPt, &mutedBrush);

        int setY = contentY + 68;

        // Group 1: RAM Allocation with Draggable Slider + Custom Numeric Input
        DrawCard(g, (float)contentX, (float)setY, (float)contentW, 148, 10, g_currentTheme.bgCard, g_currentTheme.border);

        PointF ramLblPt((float)contentX + 16, (float)setY + 14);
        g.DrawString(Tr(L"Выделенная оперативная память (RAM)", L"Allocated RAM"), -1, &fontBold, ramLblPt, &whiteBrush);

        PointF ramCustomLblPt((float)contentX + 16, (float)setY + 44);
        g.DrawString(Tr(L"Кастомный ввод:", L"Custom Input:"), -1, &fontSmall, ramCustomLblPt, &mutedBrush);

        // Edit box background card (g_hEditRam sits directly inside!)
        DrawCard(g, (float)contentX + 120, (float)setY + 38, 90, 28, 6, g_currentTheme.bgSurfaceElevated, g_currentTheme.borderFocus);
        PointF mbLblPt((float)contentX + 218, (float)setY + 44);
        g.DrawString(L"MB", -1, &fontSmallBold, mbLblPt, &mutedBrush);

        // Formatted RAM representation in GB
        float gbVal = (float)g_config.memoryMb / 1024.0f;
        std::wstringstream gbs;
        gbs << std::fixed << std::setprecision(1) << gbVal << L" GB";
        PointF gbPt((float)contentX + 250, (float)setY + 44);
        SolidBrush accentSec(g_currentTheme.accentSecondary);
        g.DrawString((L"(" + gbs.str() + L")").c_str(), -1, &fontBold, gbPt, &accentSec);

        // Interactive RAM Slider Track
        int sliderX = contentX + 16;
        int sliderY = setY + 78;
        int sliderW = contentW - 32;
        int sliderH = 8;

        GraphicsPath trackPath;
        AddRoundRect(trackPath, (float)sliderX, (float)sliderY, (float)sliderW, (float)sliderH, 4);
        SolidBrush trackBg(g_currentTheme.bgSurfaceElevated);
        g.FillPath(&trackBg, &trackPath);

        // Fill active portion of slider track
        float ramPct = (float)(g_config.memoryMb - 1024) / (float)(16384 - 1024);
        if (ramPct < 0.0f) ramPct = 0.0f;
        if (ramPct > 1.0f) ramPct = 1.0f;
        float fillW = sliderW * ramPct;

        if (fillW > 4.0f) {
            GraphicsPath fillPath;
            AddRoundRect(fillPath, (float)sliderX, (float)sliderY, fillW, (float)sliderH, 4);
            LinearGradientBrush fillGrad(PointF((float)sliderX, (float)sliderY),
                                        PointF((float)sliderX + fillW, (float)sliderY),
                                        g_currentTheme.accent, g_currentTheme.accentSecondary);
            g.FillPath(&fillGrad, &fillPath);
        }

        // Draggable Knob
        float knobX = sliderX + fillW;
        float knobY = sliderY + 4.0f;
        SolidBrush knobBrush(g_currentTheme.accentSecondary);
        g.FillEllipse(&knobBrush, knobX - 8.0f, knobY - 8.0f, 16.0f, 16.0f);
        Pen knobPen(Color(255, 255, 255, 255), 2.0f);
        g.DrawEllipse(&knobPen, knobX - 8.0f, knobY - 8.0f, 16.0f, 16.0f);

        // Quick Preset Pills
        int pillStartX = contentX + 16;
        int pillY = setY + 104;
        for (size_t i = 0; i < g_ramPresets.size(); ++i) {
            int pX = pillStartX + (int)i * 72;
            bool isSel = (g_config.memoryMb == g_ramPresets[i]);

            if (isSel) {
                DrawCard(g, (float)pX, (float)pillY, 62, 28, 6, g_currentTheme.accent, g_currentTheme.accent);
            } else {
                DrawCard(g, (float)pX, (float)pillY, 62, 28, 6, g_currentTheme.bgSurfaceElevated, g_currentTheme.border);
            }

            std::wstring ramLabel = std::to_wstring(g_ramPresets[i] / 1024) + L" GB";
            RectF ramRc((float)pX, (float)pillY, 62, 28);
            SolidBrush ramTextBrush(isSel ? Color(255, 255, 255, 255) : g_currentTheme.textMuted);
            g.DrawString(ramLabel.c_str(), -1, &fontSmallBold, ramRc, &sfCenter, &ramTextBrush);
        }

        // Group 2: Visual Themes
        int themeY = setY + 162;
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

        // Group 3: JVM Presets
        int jvmY = themeY + 104;
        DrawCard(g, (float)contentX, (float)jvmY, (float)contentW, 76, 10, g_currentTheme.bgCard, g_currentTheme.border);
        PointF jvmLblPt((float)contentX + 16, (float)jvmY + 14);
        g.DrawString(Tr(L"Предустановка флагов JVM", L"JVM Flags Preset"), -1, &fontBold, jvmLblPt, &whiteBrush);

        PointF jvmSelPt((float)contentX + 16, (float)jvmY + 42);
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

    // Left: Isometric 3D Cube & Container Info
    GraphicsPath dockAvatarPath;
    AddRoundRect(dockAvatarPath, 20, (float)dockY + 16, 52, 52, 10);
    SolidBrush dockAvatarBg(g_currentTheme.bgSurfaceElevated);
    g.FillPath(&dockAvatarBg, &dockAvatarPath);
    g.DrawPath(&dockBorderPen, &dockAvatarPath);

    DrawIconCube(g, 46.0f, (float)dockY + 42.0f, 15.0f,
                 g_currentTheme.accentSecondary, g_currentTheme.accent, Color(255, 100, 50, 160));

    PointF dNamePt(84, (float)dockY + 22);
    g.DrawString(g_activeContainerName.c_str(), -1, &fontBold, dNamePt, &whiteBrush);

    PointF dVerPt(84, (float)dockY + 44);
    std::wstring subVer = g_activeVersion + L" • " + Tr(L"Готов к игре", L"Ready to launch");
    g.DrawString(subVer.c_str(), -1, &fontSmall, dVerPt, &mutedBrush);

    // Center-Left: Prominent NICKNAME Input Box on Bottom Dock!
    PointF nickLblPt(270, (float)dockY + 14);
    SolidBrush accentSecBrush(g_currentTheme.accentSecondary);
    g.DrawString(Tr(L"НИКНЕЙМ ИГРОКА", L"PLAYER NICKNAME"), -1, &fontSmallBold, nickLblPt, &accentSecBrush);

    DrawCard(g, 268, (float)dockY + 34, 164, 32, 6, g_currentTheme.bgSurfaceElevated, g_currentTheme.borderFocus);

    // Center-Right: Version Dropdown Pill
    PointF verLblPt(452, (float)dockY + 14);
    g.DrawString(Tr(L"ВЕРСИЯ ИГРЫ", L"GAME VERSION"), -1, &fontSmallBold, verLblPt, &mutedBrush);

    DrawCard(g, 450, (float)dockY + 34, 110, 32, 6, g_currentTheme.bgSurfaceElevated, g_currentTheme.border);
    PointF verValPt(462, (float)dockY + 41);
    g.DrawString(g_activeVersion.c_str(), -1, &fontBold, verValPt, &whiteBrush);
    PointF arrowPt(538, (float)dockY + 42);
    g.DrawString(L"▼", -1, &fontSmall, arrowPt, &mutedBrush);

    // Status Pill
    int statusX = 580;
    DrawCard(g, (float)statusX, (float)dockY + 34, 210, 32, 16, g_currentTheme.bgCard, g_currentTheme.border);

    SolidBrush statusDotBrush(g_isGameRunning.load() ? g_currentTheme.warning : g_currentTheme.success);
    g.FillEllipse(&statusDotBrush, statusX + 14, dockY + 45, 10, 10);

    PointF stTextPt((float)statusX + 32, (float)dockY + 41);
    g.DrawString(g_launchStatus.c_str(), -1, &fontSmallBold, stTextPt, &whiteBrush);

    // ------------------------------------------------------------------------
    // THE PLAY BUTTON (Giant glowing rounded gradient button)
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

        // 1. Nickname Edit Box (Always on the bottom dock)
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

        // 2. Mod Search Edit Box (Mods Tab)
        g_hEditModSearch = CreateWindowExW(
            0, L"EDIT", L"",
            WS_CHILD | ES_AUTOHSCROLL,
            220 + 24 + 36, 54 + 20 + 68, 500, 24,
            hwnd, (HMENU)IDC_EDIT_MODSEARCH, g_hInstance, NULL
        );
        SendMessageW(g_hEditModSearch, WM_SETFONT, (WPARAM)hFontNick, TRUE);

        // 3. Custom RAM Numeric Input Box (Settings Tab)
        g_hEditRam = CreateWindowExW(
            0, L"EDIT", std::to_wstring(g_config.memoryMb).c_str(),
            WS_CHILD | ES_NUMBER | ES_AUTOHSCROLL,
            220 + 24 + 126, 54 + 20 + 68 + 42, 80, 22,
            hwnd, (HMENU)IDC_EDIT_RAM, g_hInstance, NULL
        );
        SendMessageW(g_hEditRam, WM_SETFONT, (WPARAM)hFontNick, TRUE);

        return 0;
    }

    case WM_COMMAND: {
        WORD notification = HIWORD(wParam);
        WORD id = LOWORD(wParam);

        if (notification == EN_CHANGE) {
            if (id == IDC_EDIT_MODSEARCH && g_hEditModSearch) {
                WCHAR buf[128] = { 0 };
                GetWindowTextW(g_hEditModSearch, buf, 127);
                g_modSearchQuery = buf;
                FilterMods();
                InvalidateRect(hwnd, NULL, FALSE);
            }
            else if (id == IDC_EDIT_RAM && g_hEditRam && !g_isDraggingSlider) {
                WCHAR buf[32] = { 0 };
                GetWindowTextW(g_hEditRam, buf, 31);
                if (wcslen(buf) > 0) {
                    int val = _wtoi(buf);
                    if (val >= 512 && val <= 32768) {
                        g_config.memoryMb = val;
                        g_containers[g_activeContainerIndex].memoryMb = val;
                        SaveConfig(g_config);
                        InvalidateRect(hwnd, NULL, FALSE);
                    }
                }
            }
        }
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
        if (g_hEditModSearch) {
            SetWindowPos(g_hEditModSearch, NULL, 220 + 24 + 36, 54 + 20 + 68, width - 220 - 48 - 146, 24, SWP_NOZORDER);
            ShowWindow(g_hEditModSearch, g_currentTab == 1 ? SW_SHOW : SW_HIDE);
        }
        if (g_hEditRam) {
            SetWindowPos(g_hEditRam, NULL, 220 + 24 + 126, 54 + 20 + 68 + 42, 80, 22, SWP_NOZORDER);
            ShowWindow(g_hEditRam, g_currentTab == 4 ? SW_SHOW : SW_HIDE);
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

        // Check sidebar tab hover (5 tabs now)
        int oldTabHover = g_hoveredTab;
        g_hoveredTab = -1;
        if (x < 220 && y >= 88 && y < 88 + 5 * 46) {
            g_hoveredTab = (y - 88) / 46;
        }

        // Check Play button hover
        int playBtnW = 190;
        int playBtnH = 52;
        int playBtnX = width - playBtnW - 24;
        int playBtnY = height - 84 + 16;
        bool oldPlayHover = g_hoverPlayBtn;
        g_hoverPlayBtn = (x >= playBtnX && x <= playBtnX + playBtnW && y >= playBtnY && y <= playBtnY + playBtnH);

        // Check RAM Slider Dragging
        if (g_isDraggingSlider && g_currentTab == 4) {
            int contentX = 220 + 24;
            int contentW = width - 220 - 48;
            int sliderX = contentX + 16;
            int sliderW = contentW - 32;

            float pct = (float)(x - sliderX) / (float)sliderW;
            if (pct < 0.0f) pct = 0.0f;
            if (pct > 1.0f) pct = 1.0f;

            int mb = 1024 + (int)(pct * (16384 - 1024));
            mb = (mb / 128) * 128; // Step by 128 MB
            if (mb < 1024) mb = 1024;
            if (mb > 16384) mb = 16384;

            if (g_config.memoryMb != mb) {
                g_config.memoryMb = mb;
                g_containers[g_activeContainerIndex].memoryMb = mb;
                if (g_hEditRam) {
                    SetWindowTextW(g_hEditRam, std::to_wstring(mb).c_str());
                }
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

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

        // Click sidebar tabs (5 tabs)
        if (x < 220 && y >= 88 && y < 88 + 5 * 46) {
            int clickedTab = (y - 88) / 46;
            if (clickedTab >= 0 && clickedTab < 5) {
                g_currentTab = clickedTab;

                if (g_hEditModSearch) {
                    ShowWindow(g_hEditModSearch, g_currentTab == 1 ? SW_SHOW : SW_HIDE);
                }
                if (g_hEditRam) {
                    ShowWindow(g_hEditRam, g_currentTab == 4 ? SW_SHOW : SW_HIDE);
                }

                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        // Click top bar active container selector pill
        if (x >= 220 + 20 && x <= 220 + 300 && y >= 11 && y <= 43) {
            if (g_containers.size() > 1) {
                int nextIdx = (g_activeContainerIndex + 1) % (int)g_containers.size();
                SetActiveContainer(nextIdx);
            } else {
                g_currentTab = 0; // Go to containers tab
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
            g_containers[g_activeContainerIndex].version = g_activeVersion;
            g_containers[g_activeContainerIndex].name = L"Minecraft " + g_activeVersion + L" Vanilla";
            g_activeContainerName = g_containers[g_activeContainerIndex].name;
            g_config.version = g_activeVersion;
            SaveConfig(g_config);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        // --- Tab 0: Containers Tab Interaction ---
        if (g_currentTab == 0) {
            int contentX = 220 + 24;
            int contentW = width - 220 - 48;
            int contentY = 54 + 20;

            // Click "+ Создать контейнер"
            int addBtnW = 180;
            int addBtnX = contentX + contentW - addBtnW;
            if (x >= addBtnX && x <= addBtnX + addBtnW && y >= contentY && y <= contentY + 34) {
                // Add a new profile
                std::wstring newId = L"c-" + std::to_wstring(g_containers.size() + 1);
                std::wstring newName = (g_containers.size() == 1) ? L"Fabric 1.20.4 Performance" : L"Forge 1.16.5 Modded";
                std::wstring newVer = (g_containers.size() == 1) ? L"1.20.4" : L"1.16.5";
                std::wstring newLdr = (g_containers.size() == 1) ? L"fabric" : L"forge";

                g_containers.push_back({ newId, newName, newVer, newLdr, 4096 });
                SetActiveContainer((int)g_containers.size() - 1);
                AppendLog(Tr(L"[Aether] Создан новый профиль: " + newName, L"[Aether] Created new container: " + newName));
                return 0;
            }

            // Click container cards or "Выбрать контейнер"
            int cardY = contentY + 68;
            int cardW = contentW;
            int cardH = 120;

            for (size_t i = 0; i < g_containers.size(); ++i) {
                int cY = cardY + (int)i * (cardH + 16);
                if (x >= contentX && x <= contentX + cardW && y >= cY && y <= cY + cardH) {
                    SetActiveContainer((int)i);
                    return 0;
                }
            }
        }

        // --- Tab 1: Mods Tab Interaction ---
        if (g_currentTab == 1) {
            int contentX = 220 + 24;
            int contentW = width - 220 - 48;
            int searchY = 54 + 20 + 62;

            // Click "Найти" button
            int findBtnX = contentX + contentW - 90;
            if (x >= findBtnX && x <= findBtnX + 90 && y >= searchY && y <= searchY + 36) {
                if (g_hEditModSearch) {
                    WCHAR buf[128] = { 0 };
                    GetWindowTextW(g_hEditModSearch, buf, 127);
                    g_modSearchQuery = buf;
                    FilterMods();
                    InvalidateRect(hwnd, NULL, FALSE);
                }
                return 0;
            }

            // Click Mod Install / Uninstall Button
            int modListY = searchY + 48;
            int modItemH = 54;
            int btnW = 120;
            int btnX = contentX + contentW - btnW - 14;

            for (size_t i = 0; i < g_filteredMods.size() && i < 6; ++i) {
                int mY = modListY + (int)i * (modItemH + 8);
                if (x >= btnX && x <= btnX + btnW && y >= mY + 12 && y <= mY + 40) {
                    g_filteredMods[i].installed = !g_filteredMods[i].installed;
                    // Sync back to master list
                    for (auto& masterMod : g_allMods) {
                        if (masterMod.name == g_filteredMods[i].name) {
                            masterMod.installed = g_filteredMods[i].installed;
                            break;
                        }
                    }

                    AppendLog(Tr(L"[Modrinth] Мод " + g_filteredMods[i].name + (g_filteredMods[i].installed ? L" успешно установлен." : L" отключен."),
                                 L"[Modrinth] Mod " + g_filteredMods[i].name + (g_filteredMods[i].installed ? L" installed." : L" disabled.")));
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
            }
        }

        // --- Tab 4: Settings Tab (RAM Slider Drag & Presets) ---
        if (g_currentTab == 4) {
            int contentX = 220 + 24;
            int contentW = width - 220 - 48;
            int setY = 54 + 20 + 68;

            // Check click on RAM slider track
            int sliderX = contentX + 16;
            int sliderY = setY + 74;
            int sliderW = contentW - 32;

            if (x >= sliderX - 10 && x <= sliderX + sliderW + 10 && y >= sliderY - 10 && y <= sliderY + 24) {
                g_isDraggingSlider = true;
                SetCapture(hwnd);

                float pct = (float)(x - sliderX) / (float)sliderW;
                if (pct < 0.0f) pct = 0.0f;
                if (pct > 1.0f) pct = 1.0f;

                int mb = 1024 + (int)(pct * (16384 - 1024));
                mb = (mb / 128) * 128;
                if (mb < 1024) mb = 1024;
                if (mb > 16384) mb = 16384;

                g_config.memoryMb = mb;
                g_containers[g_activeContainerIndex].memoryMb = mb;
                if (g_hEditRam) {
                    SetWindowTextW(g_hEditRam, std::to_wstring(mb).c_str());
                }
                SaveConfig(g_config);
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }

            // Check click on quick RAM presets
            int pillStartX = contentX + 16;
            int pillY = setY + 104;
            for (size_t i = 0; i < g_ramPresets.size(); ++i) {
                int pX = pillStartX + (int)i * 72;
                if (x >= pX && x <= pX + 62 && y >= pillY && y <= pillY + 28) {
                    g_config.memoryMb = g_ramPresets[i];
                    g_containers[g_activeContainerIndex].memoryMb = g_config.memoryMb;
                    if (g_hEditRam) {
                        SetWindowTextW(g_hEditRam, std::to_wstring(g_config.memoryMb).c_str());
                    }
                    SaveConfig(g_config);
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
            }

            // Check click on Theme presets
            int themeY = setY + 162;
            int thmStartX = contentX + 16;
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

        return 0;
    }

    case WM_LBUTTONUP: {
        if (g_isDraggingSlider) {
            g_isDraggingSlider = false;
            ReleaseCapture();
            SaveConfig(g_config);
            InvalidateRect(hwnd, NULL, FALSE);
        }

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

    // Sync default container with loaded config
    g_containers[0].version = g_config.version;
    g_containers[0].name = L"Minecraft " + g_config.version + L" Vanilla";
    g_containers[0].loader = g_config.loader;
    g_containers[0].memoryMb = g_config.memoryMb;
    g_activeContainerName = g_containers[0].name;
    g_activeVersion = g_containers[0].version;

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
