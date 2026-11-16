// ============================================================================
// Aether Launcher - Native Modern C++ Win32 GUI Engine (GDI+)
// Pure Native Desktop Application with Zero Browser / Chrome / Node Dependencies
// Features: Immersive Dark DWM Title Bar (No White Border),
// Infinite Modrinth catalog with scrolling, dependency resolution and full details,
// 21 Minecraft Versions & Dedicated Mod Loader Selector Dropdown (Vanilla/Fabric/Forge/NeoForge/Quilt),
// Global RAM Slider & Numeric Input, and In-Dock Player Nickname.
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
#include <dwmapi.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <commdlg.h>

#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <functional>
#include <set>
#include <memory>
#include <cwctype>

#include "config.hpp"
#include "minecraft_launcher.hpp"

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "comdlg32.lib")
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
        Color(255, 12, 13, 18),
        Color(255, 20, 22, 32),
        Color(255, 26, 30, 46),
        Color(255, 22, 26, 41),
        Color(70, 157, 78, 221),
        Color(255, 157, 78, 221),
        Color(255, 157, 78, 221),
        Color(255, 199, 125, 255),
        Color(100, 157, 78, 221),
        Color(255, 240, 242, 248),
        Color(255, 140, 147, 168),
        Color(255, 16, 185, 129),
        Color(255, 245, 158, 11),
        Color(255, 239, 68, 68)
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
static HWND g_hEditJvmArgs = NULL;
static HWND g_hEditJavaPath = NULL;
static HWND g_hEditGameWidth = NULL;
static HWND g_hEditGameHeight = NULL;

#define IDC_EDIT_NICKNAME   1001
#define IDC_EDIT_MODSEARCH  1002
#define IDC_EDIT_RAM        1003
#define IDC_EDIT_JVMARGS    1004
#define IDC_EDIT_JAVAPATH   1005
#define IDC_EDIT_GAMEWIDTH  1006
#define IDC_EDIT_GAMEHEIGHT 1007

static JavaInfo g_detectedJava;

static const wchar_t* PRESET_AIKAR = L"-XX:+UseG1GC -XX:+ParallelRefProcEnabled -XX:MaxGCPauseMillis=200 -XX:+UnlockExperimentalVMOptions -XX:+DisableExplicitGC -XX:+AlwaysPreTouch -XX:G1NewSizePercent=30 -XX:G1MaxNewSizePercent=40 -XX:G1ReservePercent=20 -XX:InitiatingHeapOccupancyPercent=15";
static const wchar_t* PRESET_ZGC = L"-XX:+UseZGC -XX:+ZGenerational -XX:+UnlockExperimentalVMOptions -XX:+AlwaysPreTouch";
static const wchar_t* PRESET_SHENANDOAH = L"-XX:+UseShenandoahGC -XX:+AlwaysPreTouch -XX:ShenandoahGCHeuristics=adaptive";
static const wchar_t* PRESET_G1GC = L"-XX:+UseG1GC -XX:+AlwaysPreTouch";

static LauncherConfig g_config;
// 0: Containers, 1: Mods, 2: Logs & Crashes, 3: Backups, 4: Settings
static int g_currentTab = 0;
static int g_hoveredTab = -1;
static bool g_hoverPlayBtn = false;
static bool g_pressPlayBtn = false;
static bool g_isRussian = true;
static bool g_isDraggingSlider = false;
static bool g_suppressRamChange = false;
static bool g_suppressJvmChange = false;
static bool g_suppressJavaPathChange = false;
static std::wstring g_selectedLoaderFilter = L"Все";
static std::vector<std::wstring> g_loaderFilterOptions = { L"Все", L"Fabric", L"Forge", L"NeoForge", L"Quilt" };

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
static int g_logScrollOffset = -1; // -1 = auto-scroll to bottom

// Forward declarations
bool PromptTextInput(HWND hParent, const std::wstring& title, const std::wstring& prompt, const std::wstring& initialVal, std::wstring& outVal);
static std::vector<std::wstring> GetInstalledModFiles(const std::wstring& storageDir, const std::wstring& containerId);
static void SyncInstalledModsWithDisk(const std::wstring& storageDir, const std::wstring& containerId);
void SaveLogsToFile(HWND hwnd);
void CopyLogsToClipboard(HWND hwnd);


inline std::wstring GetContainerDir(const std::wstring& storageDir, const std::wstring& containerId) {
    return MinecraftLauncher::CleanPath(storageDir + L"\\instances\\" + containerId);
}

inline std::wstring GetContainerModsDir(const std::wstring& storageDir, const std::wstring& containerId) {
    return MinecraftLauncher::CleanPath(GetContainerDir(storageDir, containerId) + L"\\mods");
}

inline void EnsureContainerStructure(const std::wstring& storageDir, const std::wstring& containerId) {
    std::wstring cDir = GetContainerDir(storageDir, containerId);
    EnsureDirectoryExists(storageDir + L"\\instances");
    EnsureDirectoryExists(cDir);
    EnsureDirectoryExists(cDir + L"\\mods");
    EnsureDirectoryExists(cDir + L"\\saves");
    EnsureDirectoryExists(cDir + L"\\screenshots");
    EnsureDirectoryExists(cDir + L"\\config");
}

// Mod Loaders
static std::vector<std::pair<std::wstring, std::wstring>> g_loaders = {
    { L"vanilla", L"Vanilla" },
    { L"fabric", L"Fabric" },
    { L"forge", L"Forge" },
    { L"neoforge", L"NeoForge" },
    { L"quilt", L"Quilt" }
};

// Expanded Minecraft Versions (21 Popular Releases)
static std::vector<std::wstring> g_versions = {
    L"1.21.3", L"1.21.1", L"1.21",
    L"1.20.6", L"1.20.4", L"1.20.2", L"1.20.1",
    L"1.19.4", L"1.19.2",
    L"1.18.2", L"1.17.1",
    L"1.16.5", L"1.15.2", L"1.14.4",
    L"1.13.2", L"1.12.2", L"1.11.2",
    L"1.10.2", L"1.9.4",
    L"1.8.9", L"1.7.10"
};
static int g_selectedVersionIndex = 1; // Default to 1.21.1

// Rich Mod Catalog (30 Top Modrinth Mods across all categories)
struct ModItem {
    std::wstring id; // Modrinth slug or project_id
    std::wstring name;
    std::wstring category;
    std::wstring downloads;
    std::wstring brief;
    std::wstring fullDesc;
    std::wstring versions;
    std::wstring loader;
    std::wstring author;
    std::wstring url;
    bool installed = false;
    bool isDownloading = false;
    int downloadProgress = 0;
};

static std::vector<ModItem> g_seedMods = {
    {
        L"sodium", L"Sodium", L"Оптимизация", L"35.4M",
        L"Современный движок рендеринга для Fabric, колоссально повышающий FPS.",
        L"Sodium — свободный открытый движок рендеринга нового поколения для Minecraft на Fabric. Он кардинально оптимизирует построение чанков, устраняет микрофризы и повышает FPS в 2-5 раз без потерь графики.",
        L"1.16.5 – 1.21.3", L"Fabric / Quilt", L"CaffeineMC", L"https://modrinth.com/mod/sodium", false
    },
    {
        L"iris", L"Iris Shaders", L"Шейдеры", L"26.1M",
        L"Современный загрузчик шейдер-паков с поддержкой PBR и совместимостью с Sodium.",
        L"Iris — современный загрузчик шейдеров с непревзойденной производительностью благодаря глубокой интеграции с Sodium. Поддерживает BSL, Complementary, Sildurs и классические OptiFine-шейдеры.",
        L"1.16.5 – 1.21.3", L"Fabric / Quilt", L"coderbot", L"https://modrinth.com/mod/iris", false
    },
    {
        L"lithium", L"Lithium", L"Оптимизация", L"21.8M",
        L"Глубокая оптимизация тика сервера, физики мобов и чанков без изменения механик.",
        L"Lithium — универсальный оптимизатор серверной физики, поиска путей ИИ мобов и загрузки чанков. Снижает нагрузку на процессор на 40% как в одиночной игре, так и на серверах.",
        L"1.16.5 – 1.21.3", L"Fabric / Quilt", L"CaffeineMC", L"https://modrinth.com/mod/lithium", false
    },
    {
        L"fabric-api", L"Fabric API", L"Библиотеки", L"48.2M",
        L"Основная библиотека хуков и событий, необходимая для большинства модов Fabric.",
        L"Fabric API — обязательная библиотека для экосистемы Fabric, предоставляющая необходимые хуки, события и интерфейсы для взаимодействия модов с игровым движком.",
        L"1.14 – 1.21.3", L"Fabric", L"FabricMC", L"https://modrinth.com/mod/fabric-api", false
    },
    {
        L"create", L"Create", L"Технологии", L"22.5M",
        L"Потрясающая кинетическая автоматизация, шестеренки, поезда и механизмы.",
        L"Create добавляет эстетичные вращающиеся шестеренки, конвейеры, ветряки, водные колеса и подвижные поезда для создания грандиозных фабрик без магических одноблочных машин.",
        L"1.18.2 – 1.20.1", L"Forge / Fabric", L"simibubi", L"https://modrinth.com/mod/create", false
    },
    {
        L"jei", L"JEI (Just Enough Items)", L"Интерфейс", L"41.9M",
        L"Удобный просмотр рецептов крафта, плавки, зельеварения и применений предметов.",
        L"JEI — стабильный, быстрый и функциональный справочник рецептов с поиском, фильтрами по модам и поддержкой закладок на боковой панели инвентаря.",
        L"1.12.2 – 1.21.3", L"Forge / Fabric / NeoForge", L"mezz", L"https://modrinth.com/mod/jei", false
    },
    {
        L"journeymap", L"JourneyMap", L"Карты", L"25.3M",
        L"Интерактивная карта мира в реальном времени с мини-картой и метками точек.",
        L"JourneyMap картографирует ваш мир во время исследования, отображает мобов, пещеры, границы чанков и позволяет телепортироваться по сохраненным путевым точкам.",
        L"1.12.2 – 1.21.3", L"Fabric / Forge", L"Techbrew", L"https://modrinth.com/mod/journeymap", false
    },
    {
        L"appleskin", L"AppleSkin", L"Интерфейс", L"36.7M",
        L"Отображение сытости, насыщения и восстановления здоровья при наведении на еду.",
        L"AppleSkin визуализирует шкалу насыщения (saturation) и истощения (exhaustion), помогая точно рассчитывать пользу от различной пищи.",
        L"1.12.2 – 1.21.3", L"Fabric / Forge / Quilt", L"squeek502", L"https://modrinth.com/mod/appleskin", false
    },
    {
        L"ferrite-core", L"FerriteCore", L"Оптимизация", L"18.9M",
        L"Снижение потребления оперативной памяти (RAM) на 30-50% без потери данных.",
        L"FerriteCore оптимизирует внутреннее хранение моделей блоков и кешей состояний в памяти, кардинально снижая аппетит тяжелых сборок модов к ОЗУ.",
        L"1.16.5 – 1.21.3", L"Fabric / Forge", L"malte0811", L"https://modrinth.com/mod/ferrite-core", false
    },
    {
        L"modmenu", L"Mod Menu", L"Интерфейс", L"34.1M",
        L"Кнопка списка модов в главном меню с просмотром описаний и экранов настроек.",
        L"Mod Menu добавляет удобный каталог установленных модов прямо в меню паузы и стартовое меню Minecraft с фильтрами, поиском и кнопками конфигураций.",
        L"1.14 – 1.21.3", L"Fabric / Quilt", L"TerraformersMC", L"https://modrinth.com/mod/modmenu", false
    },
    {
        L"distanthorizons", L"Distant Horizons", L"Шейдеры", L"10.2M",
        L"Отрисовка упрощенных далеких чанков (LOD) на дистанцию до 512+ чанков без лагов.",
        L"Distant Horizons строит потрясающие панорамы бескрайних горизонтов с минимальным влиянием на FPS благодаря системе уровней детализации (LOD).",
        L"1.18.2 – 1.21.3", L"Fabric / Forge / NeoForge", L"James Seibel", L"https://modrinth.com/mod/distanthorizons", false
    },
    {
        L"cloth-config", L"Cloth Config", L"Библиотеки", L"38.4M",
        L"Универсальный конфигурационный API для экранов настроек сотен модификаций.",
        L"Cloth Config предоставляет стандартный API интерфейса настроек для сотен популярных модификаций на Fabric и Forge.",
        L"1.16.5 – 1.21.3", L"Fabric / Forge", L"shedaniel", L"https://modrinth.com/mod/cloth-config", false
    },
    {
        L"indium", L"Indium", L"Оптимизация", L"22.1M",
        L"Слой совместимости Sodium с Fabric Rendering API (FRAPI).",
        L"Indium восстанавливает поддержку кастомных рендереров блоков для модов, использующих Fabric Rendering API совместно с Sodium.",
        L"1.16.5 – 1.21.3", L"Fabric / Quilt", L"comp500", L"https://modrinth.com/mod/indium", false
    },
    {
        L"immediatelyfast", L"ImmediatelyFast", L"Оптимизация", L"15.8M",
        L"Ускорение рендеринга элементов интерфейса, текста, частиц, карт и сундуков.",
        L"ImmediatelyFast оптимизирует непосредственный рендеринг GUI, HUD, табличек и сущностей, давая стабильный прирост FPS на загруженных экранах.",
        L"1.18.2 – 1.21.3", L"Fabric / Forge / NeoForge", L"Raphiiko", L"https://modrinth.com/mod/immediatelyfast", false
    },
    {
        L"entityculling", L"Entity Culling", L"Оптимизация", L"17.3M",
        L"Пропуск рендеринга мобов и тайлов, скрытых за непрозрачными стенами блоков.",
        L"Entity Culling использует асинхронный трассировщик лучей, чтобы не рисовать скрытые сундуки и мобов в шахтах и фермах, экономя видеопамять и FPS.",
        L"1.16.5 – 1.21.3", L"Fabric / Forge", L"tr7zw", L"https://modrinth.com/mod/entityculling", false
    },
    {
        L"xaeros-minimap", L"Xaero's Minimap", L"Карты", L"29.8M",
        L"Очень плавная и легковесная мини-карта с сохранением стиля ванильного Minecraft.",
        L"Xaero's Minimap сохраняет чистый визуальный стиль игры, отображает рельеф, пещеры, сущностей и путевые точки без просадок фреймрейта.",
        L"1.12.2 – 1.21.3", L"Fabric / Forge / NeoForge", L"xaero96", L"https://modrinth.com/mod/xaeros-minimap", false
    },
    {
        L"xaeros-world-map", L"Xaero's World Map", L"Карты", L"26.4M",
        L"Полноэкранная карта открытого мира с плавной прокруткой и масштабированием.",
        L"Xaero's World Map работает в идеальной связке с Xaero's Minimap, позволяя открывать большую интерактивную карту клавишей M.",
        L"1.12.2 – 1.21.3", L"Fabric / Forge / NeoForge", L"xaero96", L"https://modrinth.com/mod/xaeros-world-map", false
    },
    {
        L"rei", L"Roughly Enough Items (REI)", L"Интерфейс", L"24.8M",
        L"Быстрый, модульный и легко кастомизируемый браузер рецептов для Fabric.",
        L"Roughly Enough Items — легкая и мощная альтернатива JEI для Fabric и Forge с гибкой системой вкладок, фильтрации и избранного.",
        L"1.14 – 1.21.3", L"Fabric / Forge", L"shedaniel", L"https://modrinth.com/mod/rei", false
    },
    {
        L"emi", L"EMI", L"Интерфейс", L"14.2M",
        L"Современный просмотрщик рецептов с древом крафта и расчетом ингредиентов.",
        L"EMI позволяет точно рассчитывать сколько базовых ресурсов (железа, дерева, редстоуна) потребуется для создания цепочки сложных предметов.",
        L"1.18.2 – 1.21.3", L"Fabric / Forge / NeoForge", L"emilyploszaj", L"https://modrinth.com/mod/emi", false
    },
    {
        L"shulkerboxtooltip", L"Shulker Box Tooltip", L"Интерфейс", L"20.6M",
        L"Просмотр содержимого шалкеровых ящиков во всплывающей подсказке по Shift.",
        L"Позволяет мгновенно заглядывать внутрь шалкеров и мешков в инвентаре или сундуках без необходимости ставить их на землю.",
        L"1.14 – 1.21.3", L"Fabric / Forge", L"MisterPeModder", L"https://modrinth.com/mod/shulkerboxtooltip", false
    },
    {
        L"jade", L"Jade", L"Интерфейс", L"28.9M",
        L"Информативная панель с названием блока, инструментом добычи и модом.",
        L"Jade показывает точную информацию о блоке или мобе, на которого вы смотрите (здоровье, стадия роста урожая, инвентарь сундука).",
        L"1.16.5 – 1.21.3", L"Fabric / Forge / NeoForge", L"Snownee", L"https://modrinth.com/mod/jade", false
    },
    {
        L"architectury-api", L"Architectury API", L"Библиотеки", L"42.7M",
        L"Мультиплатформенная библиотека промежуточного слоя для Fabric и Forge.",
        L"Обеспечивает работу сотен кроссплатформенных модификаций, упрощая их запуск на любом загрузчике.",
        L"1.16.5 – 1.21.3", L"Fabric / Forge", L"shedaniel", L"https://modrinth.com/mod/architectury-api", false
    },
    {
        L"ae2", L"Applied Energistics 2", L"Технологии", L"19.3M",
        L"Хранение миллионов предметов в цифровой МЭ-сети и комплексный автокрафт.",
        L"AE2 позволяет оцифровывать ресурсы в виде энергии на кристаллических ячейках, объединять терминалы и строить системы автоматизации любого масштаба.",
        L"1.12.2 – 1.21.1", L"Fabric / Forge / NeoForge", L"AlgorithmX2", L"https://modrinth.com/mod/ae2", false
    },
    {
        L"farmers-delight", L"Farmer's Delight", L"Контент", L"27.8M",
        L"Кулинария, новые культуры, разделочные доски, сковороды и сочные блюда.",
        L"Farmer's Delight расширяет фермерство и готовку в Minecraft, добавляя нож, кастрюлю, компост, пироги и сытные супы с полезными баффами.",
        L"1.16.5 – 1.21.1", L"Fabric / Forge", L"vectorwing", L"https://modrinth.com/mod/farmers-delight", false
    },
    {
        L"clumps", L"Clumps", L"Оптимизация", L"33.5M",
        L"Объединение сфер опыта в один сгусток для устранения лагов на спавнерах.",
        L"Предотвращает падение FPS при убийстве сотен мобов на фермах опыта за счет склеивания шариков XP в единые сферы.",
        L"1.12.2 – 1.21.3", L"Fabric / Forge / NeoForge", L"Jaredlll08", L"https://modrinth.com/mod/clumps", false
    },
    {
        L"badoptimizations", L"BadOptimizations", L"Оптимизация", L"8.9M",
        L"Устранение узких мест в коде рендеринга блоков и сущностей Mojang.",
        L"Микрооптимизации вызовов методов ванильного клиента, ускоряющие обработку геометрии мира.",
        L"1.20.1 – 1.21.3", L"Fabric / Quilt", L"IzzelAliz", L"https://modrinth.com/mod/badoptimizations", false
    },
    {
        L"krypton", L"Krypton", L"Оптимизация", L"11.2M",
        L"Оптимизация сетевого стека Minecraft, снижающая пинг и задержку пакетов.",
        L"Заменяет тяжелые сетевые конвейеры Netty высокопроизводительными реализациями.",
        L"1.16.5 – 1.21.3", L"Fabric / Quilt", L"astei", L"https://modrinth.com/mod/krypton", false
    },
    {
        L"mouse-tweaks", L"Mouse Tweaks", L"Интерфейс", L"44.6M",
        L"Удобное перетаскивание предметов мышью с зажатыми кнопками.",
        L"Колоссально экономит время при сортировке сундуков и крафте стаков предметов.",
        L"1.12.2 – 1.21.3", L"Fabric / Forge", L"YaLTeR", L"https://modrinth.com/mod/mouse-tweaks", false
    },
    {
        L"continuity", L"Continuity", L"Шейдеры", L"12.4M",
        L"Бесшовные текстуры стекла, полированного камня и книжных полок.",
        L"Восстанавливает работу connected textures (CTM) в связке с Sodium и ресурпаками.",
        L"1.18.2 – 1.21.3", L"Fabric / Quilt", L"PepperCode1", L"https://modrinth.com/mod/continuity", false
    },
    {
        L"litematica", L"Litematica", L"Интерфейс", L"14.8M",
        L"Голографические схемы построек со списком материалов и удобной верификацией.",
        L"Litematica позволяет переносить любые постройки по схемам в одиночную игру и на серверы с подсветкой ошибок и послойным возведением.",
        L"1.14 – 1.21.3", L"Fabric / Quilt", L"maruohon", L"https://modrinth.com/mod/litematica", false
    },
    {
        L"worldedit", L"WorldEdit", L"Инструменты", L"38.5M",
        L"Мощнейший редактор мира: кисти, копирование, замена блоков и генерация форм.",
        L"Легендарный инструмент для строителей и создателей карт, позволяющий возводить горы и структуры командами за доли секунды.",
        L"1.12.2 – 1.21.3", L"Fabric / Forge", L"EngineHub", L"https://modrinth.com/mod/worldedit", false
    },
    {
        L"alexs-mobs", L"Alex's Mobs", L"Контент", L"21.2M",
        L"Более 80 уникальных анимированных животных и монстров с проработанным ИИ.",
        L"Добавляет крокодилов, горилл, китов, слонов, муравьев и фантастических существ с уникальными дропами и механиками приручения.",
        L"1.16.5 – 1.20.1", L"Forge / NeoForge", L"sbom_xela", L"https://modrinth.com/mod/alexs-mobs", false
    },
    {
        L"simple-voice-chat", L"Simple Voice Chat", L"Связь", L"29.1M",
        L"Позиционный голосовой чат с эффектом 3D-звучания, эхом в пещерах и рациями.",
        L"Позволяет общаться с друзьями голосом прямо в игре с реалистичным затуханием звука в зависимости от расстояния.",
        L"1.16.5 – 1.21.3", L"Fabric / Forge / Quilt", L"henkelmax", L"https://modrinth.com/mod/simple-voice-chat", false
    },
    {
        L"spark", L"Spark", L"Оптимизация", L"31.6M",
        L"Профайлер производительности CPU, памяти, TPS и лагов сервера и клиента.",
        L"Незаменимый инструмент для поиска модов, вызывающих зависания и фризы, с выводом подробных графиков.",
        L"1.12.2 – 1.21.3", L"Fabric / Forge / NeoForge", L"lucko", L"https://modrinth.com/mod/spark", false
    },
    {
        L"modernfix", L"ModernFix", L"Оптимизация", L"16.4M",
        L"Ускорение запуска игры в 2 раза и колоссальное уменьшение утечек памяти.",
        L"Оптимизирует фазу загрузки реестров модов и текстурных атласов, экономя время старта тяжелых сборок.",
        L"1.16.5 – 1.21.1", L"Fabric / Forge / NeoForge", L"embeddedt", L"https://modrinth.com/mod/modernfix", false
    },
    {
        L"fallingtree", L"FallingTree", L"Геймплей", L"11.8M",
        L"Срубание всего дерева целиком при разрушении одного нижнего блока бревна топором.",
        L"Удобный мод для экономии времени при заготовке древесины с поддержкой деревьев из любых модов.",
        L"1.12.2 – 1.21.3", L"Fabric / Forge", L"RakSrinaNa", L"https://modrinth.com/mod/fallingtree", false
    },
    {
        L"biomes-o-plenty", L"Biomes O' Plenty", L"Контент", L"28.4M",
        L"Десятки живописных биомов для обычного мира и Незера с новыми деревьями и рудами.",
        L"Преображает генерацию мира, добавляя вишневые рощи, лавандовые поля, хвойные леса и вулканические пустоши.",
        L"1.12.2 – 1.21.1", L"Forge / NeoForge / Fabric", L"For heck's sake", L"https://modrinth.com/mod/biomes-o-plenty", false
    },
    {
        L"security-craft", L"SecurityCraft", L"Контент", L"18.7M",
        L"Лазеры, кодовые замки, защитные двери, камеры видеонаблюдения и мины для баз.",
        L"Позволяет защитить ваш дом и сундуки от гриферов и монстров с помощью высоких технологий безопасности.",
        L"1.12.2 – 1.21.1", L"Forge / NeoForge / Fabric", L"Gefallener_Engel", L"https://modrinth.com/mod/security-craft", false
    },
    {
        L"betternether", L"BetterNether", L"Контент", L"13.5M",
        L"Колоссальное преображение Нижнего мира: биомы, светящиеся растения и новые мобы.",
        L"Делает Незер живым и захватывающим измерением с огромными грибными лесами, серными источниками и новыми блоками.",
        L"1.16.5 – 1.21.1", L"Fabric / Quilt", L"quiqueck", L"https://modrinth.com/mod/betternether", false
    },
    {
        L"betterend", L"BetterEnd", L"Контент", L"15.2M",
        L"Фантастическое преображение измерения Края с неоновыми лесами и кастомными рудами.",
        L"Превращает пустоту Энда в чарующий инопланетный мир с кристаллическими горами, светящимися деревьями и мобами.",
        L"1.16.5 – 1.21.1", L"Fabric / Quilt", L"quiqueck", L"https://modrinth.com/mod/betterend", false
    },
    {
        L"mekanism", L"Mekanism", L"Технологии", L"24.6M",
        L"Высокотехнологичные фабрики, переработка руд 5x, джетпаки, реакторы и цифровая добыча.",
        L"Mekanism — легендарный технологический мод, предоставляющий многоблочные термоядерные реакторы, лазеры, химические заводы, джетпаки и передовую броню Мека-костюма.",
        L"1.16.5 – 1.21.1", L"Forge / NeoForge", L"aidancbrady", L"https://modrinth.com/mod/mekanism", false
    },
    {
        L"botania", L"Botania", L"Магия", L"18.3M",
        L"Естественная магия цветов, генерация маны, мистические руны и древние реликвии.",
        L"Botania — необычный магический мод с акцентом на автоматизацию без традиционных числовых GUI. Вся мана и крафты управляются через лепестки цветов и распространители маны.",
        L"1.12.2 – 1.20.1", L"Forge / Fabric", L"Vazkii", L"https://modrinth.com/mod/botania", false
    },
    {
        L"terralith", L"Terralith", L"Карты", L"22.1M",
        L"Грандиозное преображение генерации мира: каньоны, вулканы и 100+ новых биомов.",
        L"Terralith кардинально преображает Overworld, создавая потрясающие реалистичные горы, пещеры и биомы без добавления новых блоков, гарантируя полную ванильную совместимость.",
        L"1.18.2 – 1.21.3", L"Fabric / Forge / Quilt", L"Starmute", L"https://modrinth.com/mod/terralith", false
    },
    {
        L"incendium", L"Incendium", L"Карты", L"11.7M",
        L"Переработка Нижнего мира: гигантские вулканы, новые замки Незера и адские биомы.",
        L"Incendium превращает Незер в опасный и захватывающий мир с новыми структурами Sanctum, крепостями пиглинов и уникальным оружием.",
        L"1.18.2 – 1.21.3", L"Fabric / Forge", L"Starmute", L"https://modrinth.com/mod/incendium", false
    },
    {
        L"nullscape", L"Nullscape", L"Карты", L"10.4M",
        L"Глубокая переработка измерения Энда: кристаллические хребты и бездны пустоты.",
        L"Nullscape делает измерение Края мистическим и опасным, возводя гигантские кристаллические пики высотой до 384 блоков.",
        L"1.18.2 – 1.21.3", L"Fabric / Forge", L"Starmute", L"https://modrinth.com/mod/nullscape", false
    },
    {
        L"twilightforest", L"The Twilight Forest", L"Приключения", L"31.8M",
        L"Таинственный Сумеречный Лес: лабиринты, замки, уникальные боссы и сокровища.",
        L"Классическое измерение вечных сумерек с Нагой, Личем, Гидрой, Ужасным Рыцарем и Замком на вершине плато ледников.",
        L"1.12.2 – 1.20.4", L"Forge / NeoForge / Fabric", L"Benimatic", L"https://modrinth.com/mod/twilightforest", false
    },
    {
        L"mowziesmobs", L"Mowzie's Mobs", L"Мобы", L"15.2M",
        L"Эпические боссы с уникальными 3D-анимациями: Баракоа, Морозные Пасти и Рыцари.",
        L"Mowzie's Mobs добавляет существ с непревзойденной проработкой анимаций, ИИ и уникальными паттернами атак.",
        L"1.12.2 – 1.20.1", L"Forge / NeoForge", L"BobMowzie", L"https://modrinth.com/mod/mowziesmobs", false
    },
    {
        L"cataclysm", L"L_Ender's Cataclysm", L"Боссы", L"13.9M",
        L"Грандиозные подземелья и боссы: Незеритовый Монстр, Левиафан и Игниус.",
        L"Добавляет смертоносные структуры и хардкорных многофазных боссов с уникальными анимациями и мощнейшим лутом.",
        L"1.16.5 – 1.20.1", L"Forge / NeoForge", L"lender_c", L"https://modrinth.com/mod/cataclysm", false
    },
    {
        L"dungeons-arise", L"When Dungeons Arise", L"Приключения", L"18.7M",
        L"Колоссальные подземелья: летающие дирижабли, храмы пустынь и заброшенные города.",
        L"Dungeons Arise генерирует масштабные структуры невиданных ранее размеров, наполненные ловушками, спавнерами и сокровищами.",
        L"1.16.5 – 1.21.1", L"Fabric / Forge", L"Aureljz", L"https://modrinth.com/mod/dungeons-arise", false
    },
    {
        L"yungs-better-mineshafts", L"YUNG's Better Mineshafts", L"Приключения", L"23.4M",
        L"Полная переработка заброшенных шахт: вариации по биомам и скрытые тоннели.",
        L"Заменяет скучные ванильные шахты динамичными лабиринтами с уникальным оформлением для каждого биома.",
        L"1.16.5 – 1.21.3", L"Fabric / Forge", L"YUNGNICKYOUNG", L"https://modrinth.com/mod/yungs-better-mineshafts", false
    },
    {
        L"yungs-better-dungeons", L"YUNG's Better Dungeons", L"Приключения", L"19.2M",
        L"Масштабная переработка спавнеров: склепы, храмы нежити и логова пауков.",
        L"Превращает маленькие спавнеры ванильного майнкрафта в захватывающие подземные комплексы.",
        L"1.16.5 – 1.21.3", L"Fabric / Forge", L"YUNGNICKYOUNG", L"https://modrinth.com/mod/yungs-better-dungeons", false
    },
    {
        L"c2me", L"C2ME (Chunk Pregenerator)", L"Оптимизация", L"9.8M",
        L"Многопоточный расчет генерации и загрузки чанков на всех ядрах CPU.",
        L"C2ME переносит генерацию чанков на все доступные потоки процессора, устраняя зависания при полете на элитрах.",
        L"1.18.2 – 1.21.3", L"Fabric / Quilt", L"ishland", L"https://modrinth.com/mod/c2me-fabric", false
    },
    {
        L"dynamic-fps", L"Dynamic FPS", L"Оптимизация", L"18.4M",
        L"Снижение частоты кадров до 1 FPS при сворачивании игры для экономии ресурсов.",
        L"Предотвращает перегрев видеокарты и снижает нагрузку на процессор, когда Minecraft свернут в трей.",
        L"1.14 – 1.21.3", L"Fabric / Forge / NeoForge", L"jellysquid_", L"https://modrinth.com/mod/dynamic-fps", false
    },
    {
        L"memoryleakfix", L"Memory Leak Fix", L"Оптимизация", L"12.1M",
        L"Исправление утечек памяти в ванильном клиенте и библиотеках модов.",
        L"Предотвращает постепенное разрастание используемой оперативной памяти во время длительных игровых сессий.",
        L"1.16.5 – 1.21.3", L"Fabric / Forge / NeoForge", L"TargetNotFound", L"https://modrinth.com/mod/memoryleakfix", false
    },
    {
        L"simply-swords", L"Simply Swords", L"Оружие", L"16.5M",
        L"Двуручные клинки, рапиры, копья, глефы и уникальные легендарные реликвии.",
        L"Simply Swords добавляет десятки новых видов холодного оружия с кастомными анимациями атак и пассивными способностями.",
        L"1.18.2 – 1.21.3", L"Fabric / Forge", L"Sweenus", L"https://modrinth.com/mod/simply-swords", false
    },
    {
        L"better-combat", L"Better Combat", L"Оружие", L"22.8M",
        L"Переработка боевой системы в стиле action-RPG: комбо-атаки, взмахи и тайминги.",
        L"Позволяет наносить красивые комбо-удары оружием с плавной анимацией взмахов и попаданий по нескольким целям.",
        L"1.18.2 – 1.21.3", L"Fabric / Forge", L"ZsoltFabian", L"https://modrinth.com/mod/better-combat", false
    },
    {
        L"mrcrayfish-furniture", L"MrCrayfish's Furniture Mod", L"Декор", L"27.3M",
        L"Современная мебель: диваны, столы, стулья, телевизоры, компьютеры и кухни.",
        L"MrCrayfish's Furniture добавляет функциональную мебель для обустройства красивых современных комнат.",
        L"1.12.2 – 1.20.1", L"Forge / NeoForge / Fabric", L"MrCrayfish", L"https://modrinth.com/mod/mrcrayfish-furniture", false
    },
    {
        L"supplementaries", L"Supplementaries", L"Декор", L"24.1M",
        L"Анимированные вывески, флюгеры, клетки для мобов, краны, фляги и декор.",
        L"Универсальный мод, гармонично дополняющий ванильный геймплей полезными декоративными и редстоун-блоками.",
        L"1.16.5 – 1.21.3", L"Fabric / Forge", L"MehVahdJukaar", L"https://modrinth.com/mod/supplementaries", false
    },
    {
        L"chipped", L"Chipped", L"Декор", L"19.8M",
        L"Тысячи новых вариантов текстур для ванильных блоков через верстак резчика.",
        L"Chipped расширяет палитру строителя, добавляя витражи, резной кирпич, узорчатое дерево и плитку.",
        L"1.16.5 – 1.21.1", L"Fabric / Forge / NeoForge", L"terrariumearth", L"https://modrinth.com/mod/chipped", false
    },
    {
        L"ambient-sounds", L"AmbientSounds", L"Звук", L"17.6M",
        L"Живое звуковое окружение: пение птиц в лесу, шелест ветра и эхо в пещерах.",
        L"AmbientSounds наполняет мир Minecraft реалистичными фоновыми звуками дикой природы и подземелий.",
        L"1.12.2 – 1.21.3", L"Fabric / Forge", L"CreativeMD", L"https://modrinth.com/mod/ambientsounds", false
    },
    {
        L"sound-physics-remastered", L"Sound Physics Remastered", L"Звук", L"15.4M",
        L"Реалистичная физика звука: эхо в больших пещерах, приглушение через стены и реверберация.",
        L"Просчитывает распространение звуковых волн в пространстве, создавая глубокое погружение в атмосферу шахт.",
        L"1.16.5 – 1.21.3", L"Fabric / Forge", L"henkelmax", L"https://modrinth.com/mod/sound-physics-remastered", false
    },
    {
        L"presence-footsteps", L"Presence Footsteps", L"Звук", L"13.2M",
        L"Богатые и реалистичные звуки шагов по различным поверхностям и материалам.",
        L"Заменяет стандартные звуки ходьбы динамичной акустической палитрой для дерева, камня, песка и травы.",
        L"1.14 – 1.21.3", L"Fabric / Quilt", L"Sollace", L"https://modrinth.com/mod/presence-footsteps", false
    },
    {
        L"sophisticated-backpacks", L"Sophisticated Backpacks", L"Инструменты", L"23.9M",
        L"Улучшаемые рюкзаки с автосортировкой, магнитом, плавкой и крафтом внутри.",
        L"Рюкзаки можно носить на спине, красить в любые цвета и оснащать десятками полезных модулей улучшений.",
        L"1.16.5 – 1.21.1", L"Forge / NeoForge / Fabric", L"P3pp3rF1y", L"https://modrinth.com/mod/sophisticated-backpacks", false
    },
    {
        L"iron-chests", L"Iron Chests", L"Инструменты", L"28.7M",
        L"Сундуки повышенной вместимости: железные, золотые, алмазные и обсидиановые.",
        L"Классический мод на компактное хранение сотен стаков предметов в пределах одного блока.",
        L"1.12.2 – 1.21.3", L"Forge / NeoForge / Fabric", L"progwml6", L"https://modrinth.com/mod/iron-chests", false
    },
    {
        L"curios", L"Curios API", L"Библиотеки", L"46.5M",
        L"Слоты аксессуаров для колец, амулетов, плащей, поясов и магических реликвий.",
        L"Стандартный фреймворк слотов экипировки, используемый сотнями модификаций на оружие и магию.",
        L"1.12.2 – 1.21.1", L"Forge / NeoForge", L"TheIllusiveC4", L"https://modrinth.com/mod/curios", false
    },
    {
        L"geckolib", L"GeckoLib", L"Библиотеки", L"41.2M",
        L"Библиотека плавной 3D-анимации моделей мобов, брони и предметов.",
        L"Предоставляет движок физической скелетной анимации для создания потрясающих существ и боссов.",
        L"1.16.5 – 1.21.3", L"Fabric / Forge / NeoForge", L"Gecko", L"https://modrinth.com/mod/geckolib", false
    },
    {
        L"patchouli", L"Patchouli", L"Библиотеки", L"39.4M",
        L"Интерактивные иллюстрированные энциклопедии и книги знаний для модов.",
        L"Позволяет создателям модификаций оформлять подробные обучающие руководства прямо внутри игры.",
        L"1.12.2 – 1.21.1", L"Forge / Fabric / NeoForge", L"Vazkii", L"https://modrinth.com/mod/patchouli", false
    },
    {
        L"pehkui", L"Pehkui", L"Библиотеки", L"20.3M",
        L"Динамическое изменение размера игрока, сущностей, предметов и хитбоксов.",
        L"Позволяет модам уменьшать игрока до размеров муравья или увеличивать до гиганта с изменением физики.",
        L"1.14 – 1.21.3", L"Fabric / Forge / Quilt", L"Virtuoel", L"https://modrinth.com/mod/pehkui", false
    },
    {
        L"polymorph", L"Polymorph", L"Интерфейс", L"19.5M",
        L"Удобный выбор рецепта при конфликте одинаковых крафтов из разных модов.",
        L"Добавляет выпадающее меню над результатом верстака для выбора желаемого предмета.",
        L"1.16.5 – 1.21.3", L"Fabric / Forge / NeoForge", L"TheIllusiveC4", L"https://modrinth.com/mod/polymorph", false
    },
    {
        L"controlling", L"Controlling", L"Интерфейс", L"32.4M",
        L"Поиск и фильтрация горячих клавиш в меню настроек управления.",
        L"Упрощает назначение кнопок в больших сборках модов и подсвечивает конфликтующие клавиши.",
        L"1.12.2 – 1.21.3", L"Forge / NeoForge / Fabric", L"Jaredlll08", L"https://modrinth.com/mod/controlling", false
    },
    {
        L"searchables", L"Searchables", L"Библиотеки", L"25.1M",
        L"Универсальный компонент строки поиска для любых экранов инвентаря и настроек.",
        L"Вспомогательная библиотека фильтрации элементов для сотен модификаций интерфейса.",
        L"1.18.2 – 1.21.3", L"Fabric / Forge / NeoForge", L"Jaredlll08", L"https://modrinth.com/mod/searchables", false
    },
    {
        L"macaws-bridges", L"Macaw's Bridges", L"Декор", L"21.6M",
        L"Красивые деревянные, каменные и веревочные подвесные мосты для рек и каньонов.",
        L"Позволяет легко соединять берега аккуратными мостами с перилами различных стилей.",
        L"1.12.2 – 1.21.3", L"Fabric / Forge / NeoForge", L"sketch_macaw", L"https://modrinth.com/mod/macaws-bridges", false
    },
    {
        L"macaws-doors", L"Macaw's Doors", L"Декор", L"18.9M",
        L"Японские сёдзи, металлические люки, двери таверны и современные стеклянные двери.",
        L"Расширяет набор дверей десятками новых вариаций со звуками и анимациями открытия.",
        L"1.12.2 – 1.21.3", L"Fabric / Forge / NeoForge", L"sketch_macaw", L"https://modrinth.com/mod/macaws-doors", false
    },
    {
        L"macaws-windows", L"Macaw's Windows", L"Декор", L"17.4M",
        L"Открывающиеся окна, ставни, мозаичные витражи и жалюзи для домов.",
        L"Делает окна реалистичными, позволяя распахивать рамы кликом правой кнопки мыши.",
        L"1.12.2 – 1.21.3", L"Fabric / Forge / NeoForge", L"sketch_macaw", L"https://modrinth.com/mod/macaws-windows", false
    },
    {
        L"macaws-roofs", L"Macaw's Roofs", L"Декор", L"16.8M",
        L"Угловые крыши, черепица, водостоки и мансардные скаты для построек.",
        L"Устраняет ступенчатость крыш ванильного майнкрафта с помощью плавных покатых блоков.",
        L"1.12.2 – 1.21.3", L"Fabric / Forge / NeoForge", L"sketch_macaw", L"https://modrinth.com/mod/macaws-roofs", false
    },
    {
        L"handcrafted", L"Handcrafted", L"Декор", L"14.7M",
        L"Эстетичная резная мебель, подушки, полки, глиняная посуда и картины.",
        L"Уютная средневековая и фэнтезийная мебель с текстурами высокого качества.",
        L"1.18.2 – 1.21.1", L"Fabric / Forge", L"terrariumearth", L"https://modrinth.com/mod/handcrafted", false
    },
    {
        L"visuality", L"Visuality", L"Шейдеры", L"11.9M",
        L"Маленькие частицы искр, падающих листьев, брызг воды и светлячков.",
        L"Оживляет мир деликатными визуальными эффектами частиц без влияния на производительность.",
        L"1.18.2 – 1.21.3", L"Fabric / Quilt", L"Juuz", L"https://modrinth.com/mod/visuality", false
    },
    {
        L"fallingleaves", L"Falling Leaves", L"Шейдеры", L"13.8M",
        L"Красивое опадание разноцветных листьев с крон деревьев по ветру.",
        L"Добавляет атмосферные кружащиеся листья под березами, дубами и сакурами.",
        L"1.16.5 – 1.21.3", L"Fabric / Forge", L"CheaterMad", L"https://modrinth.com/mod/fallingleaves", false
    },
    {
        L"3dskinlayers", L"3D Skin Layers", L"Интерфейс", L"19.2M",
        L"Объемный 3D-рендеринг второго слоя скина игрока вместо плоской текстуры.",
        L"Делает воротники, очки, капюшоны и прически объемными и реалистичными.",
        L"1.16.5 – 1.21.3", L"Fabric / Forge", L"tr7zw", L"https://modrinth.com/mod/3dskinlayers", false
    },
    {
        L"wavey-capes", L"Wavey Capes", L"Интерфейс", L"14.1M",
        L"Плавная физика развевающихся на ветру плащей с реалистичной гравитацией.",
        L"Заменяет картонные плащи Minecraft мягкой анимацией ткани при беге и прыжках.",
        L"1.16.5 – 1.21.3", L"Fabric / Forge", L"tr7zw", L"https://modrinth.com/mod/waveycapes", false
    },
    {
        L"not-enough-animations", L"Not Enough Animations", L"Интерфейс", L"15.9M",
        L"Анимации третьего лица: еда, питье, гребля, лазание по лестницам и чтение карт.",
        L"Синхронизирует движения тела персонажа с действиями от первого лица для друзей по мультиплееру.",
        L"1.16.5 – 1.21.3", L"Fabric / Forge", L"tr7zw", L"https://modrinth.com/mod/not-enough-animations", false
    },
    {
        L"first-person-model", L"First-person Model", L"Интерфейс", L"12.6M",
        L"Видимость рук, ног и тела персонажа при взгляде вниз от первого лица.",
        L"Позволяет видеть свое снаряжение и броню при взгляде под ноги.",
        L"1.16.5 – 1.21.3", L"Fabric / Forge", L"tr7zw", L"https://modrinth.com/mod/first-person-model", false
    },
    {
        L"reese-sodium-options", L"Reese's Sodium Options", L"Оптимизация", L"21.4M",
        L"Удобный интерфейс настроек видео для Sodium с вертикальными вкладками и прокруткой.",
        L"Заменяет стандартное меню настроек Sodium современным адаптивным экраном.",
        L"1.16.5 – 1.21.3", L"Fabric / Quilt", L"FlashyReese", L"https://modrinth.com/mod/reeses-sodium-options", false
    },
    {
        L"itemphysic", L"ItemPhysic", L"Геймплей", L"16.1M",
        L"Реалистичная физика упавших предметов: бросание, скольжение и плавучесть.",
        L"Предметы реалистично падают на землю, не вращаясь в воздухе, и плавают на поверхности воды.",
        L"1.12.2 – 1.20.1", L"Forge / Fabric", L"CreativeMD", L"https://modrinth.com/mod/itemphysic", false
    },
    {
        L"fast-ip-ping", L"Fast IP Ping", L"Оптимизация", L"8.2M",
        L"Мгновенный пинг серверов в сетевой игре без зависания меню паузы.",
        L"Оптимизирует асинхронные сокеты опроса состояния серверов в списке миров.",
        L"1.18.2 – 1.21.3", L"Fabric / Quilt", L"astei", L"https://modrinth.com/mod/fast-ip-ping", false
    },
    {
        L"aquaculture", L"Aquaculture 2", L"Контент", L"18.4M",
        L"Более 30 новых видов рыб для разных биомов, наживки, снасти и сокровища в воде.",
        L"Превращает рыбалку в увлекательную охоту на редкую рыбу со спиннингами и крючками.",
        L"1.12.2 – 1.21.1", L"Forge / NeoForge / Fabric", L"Shadows_of_Fire", L"https://modrinth.com/mod/aquaculture", false
    },
    {
        L"the-aether", L"The Aether", L"Приключения", L"29.4M",
        L"Легендарный Рай: парящие небесные острова, моа, бронзовые и серебряные подземелья.",
        L"Портал из светящегося камня и воды переносит в Небесный мир с летающими китами и боссом Солнечного Духа.",
        L"1.12.2 – 1.20.4", L"Forge / NeoForge", L"GildedGames", L"https://modrinth.com/mod/aether", false
    },
    {
        L"undergarden", L"The Undergarden", L"Приключения", L"11.5M",
        L"Глубокий подземный мир вечной тьмы: новые руды, древние звери и ядовитые туманы.",
        L"Мрачное измерение с уникальной атмосферой древних подземных глубин.",
        L"1.16.5 – 1.20.1", L"Forge / NeoForge", L"Quek04", L"https://modrinth.com/mod/undergarden", false
    },
    {
        L"deeperdarker", L"Deeper and Darker", L"Приключения", L"14.3M",
        L"Измерение Древнего Города за порталом Вардена: Другие Берега и эхо-сокровища.",
        L"Активируйте центральный портал Древнего города в Deep Dark и исследуйте мир Скалка.",
        L"1.19.2 – 1.21.1", L"Fabric / Forge / NeoForge", L"Kyron", L"https://modrinth.com/mod/deeperdarker", false
    },
    {
        L"relics", L"Relics", L"Приключения", L"13.7M",
        L"Десятки уникальных магических реликвий и артефактов со сложной прокачкой способностей.",
        L"Находите древние кольца, ботинки левитации и амулеты в сундуках сокровищниц.",
        L"1.16.5 – 1.20.1", L"Forge / NeoForge / Fabric", L"SSKirillS", L"https://modrinth.com/mod/relics", false
    },
    {
        L"artifacts", L"Artifacts", L"Приключения", L"21.5M",
        L"Мощные артефакты в подземных лабиринтах: когти копателя, спасательные жилеты и шарфы.",
        L"Каждый артефакт дает постоянный полезный пассивный эффект при экипировке в слот Curios.",
        L"1.16.5 – 1.21.1", L"Forge / NeoForge / Fabric", L"ochotonida", L"https://modrinth.com/mod/artifacts", false
    },
    {
        L"irons-spells", L"Iron's Spells 'n Spellbooks", L"Магия", L"17.8M",
        L"Классическая магия RPG: книги заклинаний, школы льда, огня, молний и святости.",
        L"Крафтите мантии чародея, находите древние свитки и применяйте сотни зрелищных заклинаний.",
        L"1.18.2 – 1.20.1", L"Forge / NeoForge", L"Iron431", L"https://modrinth.com/mod/irons-spells-n-spellbooks", false
    },
    {
        L"ars-nouveau", L"Ars Nouveau", L"Магия", L"16.2M",
        L"Конструктор заклинаний: создавайте собственные чары из глифов, призывайте фамильяров.",
        L"Универсальная магическая система, позволяющая создавать заклинания для копки, боя и защиты.",
        L"1.16.5 – 1.21.1", L"Forge / NeoForge", L"baileyholl2", L"https://modrinth.com/mod/ars-nouveau", false
    },
    {
        L"thermal-expansion", L"Thermal Expansion", L"Технологии", L"26.7M",
        L"Энергия Redstone Flux (RF), индукционные плавильни, измельчители и трубы.",
        L"Классический и стабильный мод технологической индустрии для комфортной переработки ресурсов.",
        L"1.12.2 – 1.20.1", L"Forge / NeoForge", L"TeamCoFH", L"https://modrinth.com/mod/thermal-expansion", false
    },
    {
        L"enderio", L"Ender IO", L"Технологии", L"22.3M",
        L"Компактные кабельные каналы в одном блоке, генераторы, фермы и телепорты.",
        L"Позволяет прокладывать трубы энергии, предметов, жидкостей и редстоуна внутри единого блока.",
        L"1.12.2 – 1.20.1", L"Forge / NeoForge", L"SleepyTrousers", L"https://modrinth.com/mod/enderio", false
    },
    {
        L"refined-storage", L"Refined Storage", L"Технологии", L"21.8M",
        L"Элегантная система цифрового хранения предметов и жидкостей в сетевых дисках.",
        L"Удобная и простая в настройке альтернатива AE2 с беспроводным доступом к инвентарю.",
        L"1.12.2 – 1.21.1", L"Forge / NeoForge / Fabric", L"raoulvdberge", L"https://modrinth.com/mod/refined-storage", false
    },
    {
        L"powah", L"Powah! (Rearchitected)", L"Технологии", L"18.1M",
        L"Многоуровневые генераторы энергии: реакторы, термогенераторы и солнечные панели.",
        L"Красивые и эффективные энергетические установки от базового уровня до уровня Нитро.",
        L"1.16.5 – 1.21.1", L"Forge / NeoForge / Fabric", L"Technici4n", L"https://modrinth.com/mod/powah", false
    },
    {
        L"immersive-engineering", L"Immersive Engineering", L"Технологии", L"23.5M",
        L"Индустриальная эстетика ретро-футуризма: висячие провода, дробители и конвейеры.",
        L"Многоблочные реалистичные механизмы с видимыми шестеренками и кабелями высокого напряжения.",
        L"1.12.2 – 1.20.1", L"Forge / NeoForge", L"BluSunrize", L"https://modrinth.com/mod/immersive-engineering", false
    },
    {
        L"cooking-for-blockheads", L"Cooking for Blockheads", L"Контент", L"25.2M",
        L"Модульная кухня: холодильник, плита, тостер и книга рецептов с автоподбором блюд.",
        L"Облегчает кулинарию, автоматически объединяя ингредиенты из всех шкафов для готовки блюд.",
        L"1.12.2 – 1.21.1", L"Forge / NeoForge / Fabric", L"BlayTheNinth", L"https://modrinth.com/mod/cooking-for-blockheads", false
    },
    {
        L"croptopia", L"Croptopia", L"Контент", L"14.6M",
        L"Более 80 новых культур, фруктовые деревья, специи и сотни аппетитных блюд.",
        L"Добавляет артишоки, авокадо, корицу, хмель, базилик, пиццы, бургеры и смузи.",
        L"1.16.5 – 1.21.3", L"Fabric / Forge", L"Thonk", L"https://modrinth.com/mod/croptopia", false
    },
    {
        L"nether-depths-upgrade", L"Nether Depths Upgrade", L"Контент", L"6.8M",
        L"Лавовая рыбалка в лавовых озерах Незера со специальными жаропрочными удочками.",
        L"Добавляет адскую рыбу, магматических окуней и уникальные артефакты лавовых глубин.",
        L"1.18.2 – 1.20.1", L"Forge / Fabric", L"Scimiguy", L"https://modrinth.com/mod/nether-depths-upgrade", false
    },
    {
        L"regions-unexplored", L"Regions Unexplored", L"Карты", L"12.4M",
        L"Более 70 живописных биомов: эвкалиптовые леса, бамбуковые горы и вересковые пустоши.",
        L"Добавляет уникальные типы древесины, камня и растительности для исследователей природы.",
        L"1.18.2 – 1.21.1", L"Forge / Fabric / NeoForge", L"UglyToad", L"https://modrinth.com/mod/regions-unexplored", false
    },
    {
        L"tectonic", L"Tectonic", L"Карты", L"15.8M",
        L"Горные массивы колоссального масштаба, плавные плато и протяженные каньоны рек.",
        L"Модификатор генератора ландшафта, создающий грандиозные скалы и глубокие фьорды.",
        L"1.18.2 – 1.21.3", L"Fabric / Forge / Quilt", L"Apollo", L"https://modrinth.com/mod/tectonic", false
    },
    {
        L"axiom", L"Axiom", L"Инструменты", L"11.2M",
        L"Профессиональный внутриигровой 3D-редактор для архитекторов и создателей миров.",
        L"Объединяет возможности Blender и WorldEdit в удобном современном графическом интерфейсе.",
        L"1.20.1 – 1.21.3", L"Fabric", L"Moulberry", L"https://modrinth.com/mod/axiom", false
    },
    {
        L"minihud", L"MiniHUD", L"Интерфейс", L"14.5M",
        L"Информационный оверлей: точные координаты, FPS, время суток, освещение и чанки.",
        L"Легковесный HUD без перегрузки экрана, позволяющий выводить только нужные параметры.",
        L"1.14 – 1.21.3", L"Fabric / Quilt", L"maruohon", L"https://modrinth.com/mod/minihud", false
    },
    {
        L"tweakeroo", L"Tweakeroo", L"Инструменты", L"13.1M",
        L"Десятки удобных твиков: быстрая смена слотов, зум, авто-клик и размещение блоков.",
        L"Набор полезных улучшений удобства взаимодействия с миром для строителей и редстоунеров.",
        L"1.14 – 1.21.3", L"Fabric / Quilt", L"maruohon", L"https://modrinth.com/mod/tweakeroo", false
    },
    {
        L"betterf3", L"BetterF3", L"Интерфейс", L"18.7M",
        L"Красивый, цветной и структурированный экран отладки F3 с возможностью скрывать лишнее.",
        L"Разделяет экран F3 на аккуратные цветные модули, делая поиск информации мгновенным.",
        L"1.16.5 – 1.21.3", L"Fabric / Forge / NeoForge", L"compscistudent", L"https://modrinth.com/mod/betterf3", false
    },
    {
        L"inventory-profiles-next", L"Inventory Profiles Next", L"Интерфейс", L"21.3M",
        L"Интеллектуальная сортировка сундуков и инвентаря в 1 клик с профилями раскладок.",
        L"Быстрая сортировка предметов по колонкам, категориям и прочности инструмента.",
        L"1.16.5 – 1.21.3", L"Fabric / Forge", L"mirinapp", L"https://modrinth.com/mod/inventory-profiles-next", false
    },
    {
        L"vulkanmod", L"VulkanMod", L"Оптимизация", L"8.9M",
        L"Перенос графического конвейера Minecraft с OpenGL на современный Vulkan API.",
        L"Дает колоссальный прирост кадров на видеокартах AMD и Intel с современной поддержкой Vulkan.",
        L"1.18.2 – 1.21.1", L"Fabric", L"xCollateral", L"https://modrinth.com/mod/vulkanmod", false
    },
    {
        L"nvidium", L"Nvidium", L"Оптимизация", L"10.6M",
        L"Аппаратный рендеринг геометрии через Nvidia Mesh Shaders для тысяч FPS.",
        L"Использует уникальные возможности архитектур Turing и Ampere для отрисовки мира со скоростью 1000+ FPS.",
        L"1.20.1 – 1.21.3", L"Fabric", L"cortex", L"https://modrinth.com/mod/nvidium", false
    },
    {
        L"fastsuite", L"FastSuite", L"Оптимизация", L"14.2M",
        L"Кеширование рецептов крафта для мгновенной обработки в верстаках и печах.",
        L"Устраняет микрозадержки при быстром шифт-клике стаков предметов в инвентаре.",
        L"1.16.5 – 1.21.1", L"Forge / NeoForge / Fabric", L"Shadows_of_Fire", L"https://modrinth.com/mod/fastsuite", false
    },
    {
        L"dash", L"Dash Loader", L"Оптимизация", L"7.4M",
        L"Кеширование ассетов и моделей для загрузки лаунчером за доли секунды.",
        L"Сохраняет оптимизированные бинарные слепки ресурсов, ускоряя повторные запуски.",
        L"1.18.2 – 1.20.4", L"Fabric", L"DeadlyMC", L"https://modrinth.com/mod/dashloader", false
    },
    {
        L"naturalist", L"Naturalist", L"Мобы", L"16.8M",
        L"Живая фауна: медведи, олени, змеи, птицы, бабочки, львы и жирафы.",
        L"Наполняет леса и саванны реалистичными животными с естественными цепочками питания.",
        L"1.18.2 – 1.21.1", L"Fabric / Forge", L"Starfish_Studios", L"https://modrinth.com/mod/naturalist", false
    },
    {
        L"friends-and-foes", L"Friends & Foes", L"Мобы", L"14.9M",
        L"Возвращение забытых существ из голосований Mob Vote: Иллюзор, Ледяной Медведь и Медный Голем.",
        L"Добавляет мобов, не вошедших в ванильные обновления, с каноничным качеством исполнения.",
        L"1.18.2 – 1.21.3", L"Fabric / Forge", L"Faboslav", L"https://modrinth.com/mod/friends-and-foes", false
    },
    {
        L"towns-and-towers", L"Towns and Towers", L"Приключения", L"17.3M",
        L"Великолепные деревни и аванпосты разбойников, адаптированные под каждый биом.",
        L"Деревни в джунглях, на пляжах, в горах и пустошах с уникальной архитектурой и жителями.",
        L"1.18.2 – 1.21.3", L"Fabric / Forge", L"BibIicIe", L"https://modrinth.com/mod/towns-and-towers", false
    },
    {
        L"repurposed-structures", L"Repurposed Structures", L"Приключения", L"22.4M",
        L"Вариации ванильных храмов, крепостей и хижин ведьм для всех типов биомов.",
        L"Крепости Незера в обычном мире, храмы в джунглях из песчаника и заброшенные шахты океана.",
        L"1.16.5 – 1.21.3", L"Fabric / Forge", L"TelepathicGrunt", L"https://modrinth.com/mod/repurposed-structures", false
    },
    {
        L"echo-chest", L"Echo Chest", L"Инструменты", L"5.8M",
        L"Сундук скалка, связывающий квантовые хранилища между измерениями.",
        L"Позволяет передавать ресурсы из края и незера напрямую на базу без задержек.",
        L"1.19.2 – 1.21.1", L"Fabric / Forge", L"Enderite", L"https://modrinth.com/mod/echo-chest", false
    },
    {
        L"xaeros-minimap-fairplay", L"Xaero's Minimap FairPlay", L"Карты", L"18.2M",
        L"Справедливая версия мини-карты без показа пещер и игроков для честных серверов.",
        L"Разрешена на строгих PvP и RP серверах, где запрещен показ скрытых пещер и радаров сущностей.",
        L"1.12.2 – 1.21.3", L"Fabric / Forge", L"xaero96", L"https://modrinth.com/mod/xaeros-minimap-fairplay", false
    },
    {
        L"enhanced-block-entities", L"Enhanced Block Entities", L"Оптимизация", L"14.5M",
        L"Оптимизация сундуков, колоколов и табличек с переходом на рендеринг блоков.",
        L"Устраняет лаги на больших складах с сотнями сундуков, заменяя медленные Entity на Block-модели.",
        L"1.17.1 – 1.21.3", L"Fabric / Quilt", L"FoundationGames", L"https://modrinth.com/mod/ebe", false
    },
    // --- Expanded Additional Modrinth Top Mods ---
    {
        L"sodium-extra", L"Sodium Extra", L"Графика", L"18.3M",
        L"Расширенные настройки графики, анимаций и детализации для Sodium.",
        L"Sodium Extra возвращает большинство графических опций из OptiFine в интерфейс Sodium: настройку частиц, анимаций, деталей облаков, звезд и рендера мира.",
        L"1.16.5 – 1.21.3", L"Fabric / Quilt", L"FlashyReese", L"https://modrinth.com/mod/sodium-extra", false
    },
    {
        L"entity-view-distance", L"Entity View Distance", L"Оптимизация", L"8.7M",
        L"Динамическое масштабирование дальности прорисовки мобов и существ для максимального FPS.",
        L"Увеличивает FPS в зонах с большим скоплением мобов, настраивая дистанцию рендеринга существ независимо от дальности чанков.",
        L"1.16.5 – 1.21.3", L"Fabric / Forge / NeoForge", L"Mishole", L"https://modrinth.com/mod/entity-view-distance", false
    },
    {
        L"bobby", L"Bobby", L"Графика", L"9.2M",
        L"Отображение чанков мира за пределами ограничения дистанции сервера.",
        L"Bobby кэширует чанки на стороне клиента и позволяет выставить дальность прорисовки до 32 чанков даже на серверах с ограничением 8 чанков.",
        L"1.16.5 – 1.21.3", L"Fabric / Quilt", L"Johni0702", L"https://modrinth.com/mod/bobby", false
    },
    {
        L"wthit", L"WTHIT (What The Hell Is That?)", L"Интерфейс", L"16.5M",
        L"Информативный всплывающий HUD с названием блока, здоровьем моба и содержимым инвентаря.",
        L"Легковесный и невероятно быстрый аналог WAILA и Jade с поддержкой плагинов для всех технических модов.",
        L"1.16.5 – 1.21.3", L"Fabric / Forge / Quilt", L"badasintended", L"https://modrinth.com/mod/wthit", false
    },
    {
        L"chat-heads", L"Chat Heads", L"Интерфейс", L"14.1M",
        L"Отображение аватара и скина игрока рядом с его сообщениями в чате.",
        L"Делает чат на серверах и в одиночной игре намного удобнее и красивее, показывая 3D-голову автора каждого сообщения.",
        L"1.16.5 – 1.21.3", L"Fabric / Forge / NeoForge", L"dzwdz", L"https://modrinth.com/mod/chat-heads", false
    },
    {
        L"carpet", L"Fabric Carpet", L"Утилиты", L"11.4M",
        L"Мощнейший инструмент для технического выживания, спавна мобов и тестирования ферм от gnembon.",
        L"Carpet добавляет гибкие правила для контроля тиков, визуализации спавна, заморозки игры и подробной статистики TPS.",
        L"1.14 – 1.21.3", L"Fabric", L"gnembon", L"https://modrinth.com/mod/carpet", false
    },
    {
        L"camera-utils", L"Camera Utils", L"Утилиты", L"7.6M",
        L"Свободная камера, зум с колесиком мыши, фиксация угла обзора и третье лицо.",
        L"Удобный мод для скриншотов, записи видео и кинематографичного обзора построек без сторонних читов.",
        L"1.18.2 – 1.21.3", L"Fabric / Forge / NeoForge", L"henkelmax", L"https://modrinth.com/mod/camera-utils", false
    },
    {
        L"physics-mod", L"Physics Mod", L"Графика", L"12.8M",
        L"Реалистичная физика разрушения блоков, тряпичные куклы мобов и динамичный дым.",
        L"Физический движок для Minecraft: блоки рассыпаются на физические осколки, мобы падают по законам физики ragdoll, а жидкости красиво колышутся.",
        L"1.16.5 – 1.21.3", L"Fabric / Forge / NeoForge", L"Haubna", L"https://modrinth.com/mod/physics-mod", false
    },
    {
        L"valkyrien-skies", L"Valkyrien Skies", L"Технологии", L"8.9M",
        L"Реальная физика для летающих кораблей, подводных лодок и движущихся структур.",
        L"Мод превращает ваши постройки в полноценные физические объекты: стройте дирижабли, лифты и пароходы, на которых можно ходить.",
        L"1.18.2 – 1.20.1", L"Forge / Fabric", L"Trique", L"https://modrinth.com/mod/valkyrien-skies", false
    },
    {
        L"eureka", L"Eureka! Ships", L"Технологии", L"7.4M",
        L"Создание управляемых дирижаблей и кораблей на базе Valkyrien Skies.",
        L"Поставьте штурвал, двигатели и паруса на любую вашу деревянную или каменную конструкцию и отправляйтесь в плавание по воздуху и воде.",
        L"1.18.2 – 1.20.1", L"Forge / Fabric", L"Rubydesic", L"https://modrinth.com/mod/eureka", false
    },
    {
        L"draconic-evolution", L"Draconic Evolution", L"Технологии", L"9.8M",
        L"Легендарный эндгейм-мод с ядерными реакторами, драконьей броней и хранилищами энергии.",
        L"Добавляет драконий инфузор, многомиллиардные энергетические ядра и мощнейшую экипировку для абсолютного доминирования.",
        L"1.12.2 – 1.20.1", L"Forge / NeoForge", L"brandon3055", L"https://modrinth.com/mod/draconic-evolution", false
    },
    {
        L"projecte", L"ProjectE", L"Магия", L"15.3M",
        L"Эквивалентный обмен (Equivalent Exchange), трансмутация материи и энергия EMC.",
        L"Позволяет превращать одни ресурсы в другие через очки EMC с помощью философского камня и столов трансмутации.",
        L"1.12.2 – 1.20.1", L"Forge / NeoForge", L"sinkillerj", L"https://modrinth.com/mod/projecte", false
    },
    {
        L"pneumaticcraft-repressurized", L"PneumaticCraft: Repressurized", L"Технологии", L"7.1M",
        L"Инженерная пневматика: сжатый воздух, дроны-логисты, пластик и сборочные манипуляторы.",
        L"Постройте сложную пневматическую систему давления для автоматизации ферм, защиты базы турелями и программирования дронов.",
        L"1.16.5 – 1.20.1", L"Forge / NeoForge", L"desht364", L"https://modrinth.com/mod/pneumaticcraft-repressurized", false
    },
    {
        L"industrial-foregoing", L"Industrial Foregoing", L"Технологии", L"11.9M",
        L"Полная промышленная автоматизация: автофермы дерева, переработка латекса и лазерные буры.",
        L"Современное продолжение культового MineFactory Reloaded со стильным интерфейсом и огромным набором промышленных генераторов.",
        L"1.12.2 – 1.20.1", L"Forge / NeoForge", L"Buuz135", L"https://modrinth.com/mod/industrial-foregoing", false
    },
    {
        L"cc-tweaked", L"CC: Tweaked (ComputerCraft)", L"Технологии", L"13.7M",
        L"Программируемые компьютеры, мониторы и роботы-черепашки на языке Lua.",
        L"Пишите реальные скрипты на Lua в игре для автоматической добычи руды, беспроводных чатов, автоматизации сундуков и управления играми.",
        L"1.12.2 – 1.21.1", L"Forge / Fabric / NeoForge", L"SquidDev", L"https://modrinth.com/mod/cc-tweaked", false
    },
    {
        L"ad-astra", L"Ad Astra", L"Приключения", L"12.4M",
        L"Космические полеты: ракеты, скафандры, базы на Луне, Марсе, Венере, Меркурии и Гласио.",
        L"Постройте ракету первой ступени, синтезируйте кислород и исследуйте инопланетные планеты с уникальной гравитацией и рудами.",
        L"1.18.2 – 1.20.4", L"Forge / Fabric / NeoForge", L"Terrarium Earth", L"https://modrinth.com/mod/ad-astra", false
    },
    {
        L"ice-and-fire", L"Ice and Fire: Dragons", L"Мобы", L"17.6M",
        L"Огненные, ледяные и молниеносные драконы, циклопы, сирены, горгоны и гиппогрифы.",
        L"Масштабный мод на мифологических существ с возможностью приручить дракона, ковать драконью сталь и сражаться с могучими боссами.",
        L"1.12.2 – 1.20.1", L"Forge / NeoForge", L"Alexthe666", L"https://modrinth.com/mod/ice-and-fire-dragons", false
    },
    {
        L"born-in-chaos", L"Born in Chaos", L"Мобы", L"9.5M",
        L"Пугающие ночные кошмары, темные культисты, мистические мобы и элитное оружие.",
        L"Наполняет мир опасными ночными существами, новыми подземельями и ритуалами темной магии для хардкорного выживания.",
        L"1.18.2 – 1.20.1", L"Forge / NeoForge", L"monte_4", L"https://modrinth.com/mod/born-in-chaos", false
    },
    {
        L"mutant-monsters", L"Mutant Monsters", L"Мобы", L"11.2M",
        L"Возрождение легендарных мобов-мутантов: Мутант-Зомби, Мутант-Крипер, Скелет и Эндермен.",
        L"Культовые грозные мутанты с сокрушительными анимациями, бросками блоков и уникальным оружием, выпадающим при победе.",
        L"1.16.5 – 1.20.1", L"Forge / NeoForge / Fabric", L"Fuzs", L"https://modrinth.com/mod/mutant-monsters", false
    },
    {
        L"alexs-caves", L"Alex's Caves", L"Приключения", L"10.8M",
        L"Грандиозные подземные биомы: магнитные пещеры, ядовитые пустоши и доисторические динозавры.",
        L"Шедевр пещерной генерации от автора Alex's Mobs с ядерными боеголовками, подводными субмаринами и древними доисторическими существами.",
        L"1.20.1", L"Forge / NeoForge", L"sbom_xela", L"https://modrinth.com/mod/alexs-caves", false
    },
    {
        L"blood-magic", L"Blood Magic", L"Магия", L"12.1M",
        L"Алхимия жизненной силы, алтари крови, демонические ритуалы и связывающая магия.",
        L"Используйте жизненную эссенцию (LP) для создания заклинаний, ритуалов автоматизации и призыва духов.",
        L"1.12.2 – 1.20.1", L"Forge / NeoForge", L"WayofTime", L"https://modrinth.com/mod/blood-magic", false
    },
    {
        L"macaws-fences", L"Macaw's Fences and Walls", L"Декор", L"14.5M",
        L"Современные заборы, проволочные ограждения, деревянные частоколы и барьеры.",
        L"Огромный набор декоративных оград для стильного оформления вашей деревни, фермы и замка.",
        L"1.12.2 – 1.21.3", L"Fabric / Forge / NeoForge", L"sketch_macaw", L"https://modrinth.com/mod/macaws-fences", false
    },
    {
        L"connected-glass", L"Connected Glass", L"Декор", L"16.7M",
        L"Бесшовные соединенные текстуры стекла без назойливых внутренних рамок.",
        L"Добавляет чистое, соединенное, тонированное и матовое стекло, гармонично соединяющееся между собой без OptiFine.",
        L"1.12.2 – 1.21.3", L"Fabric / Forge / NeoForge", L"SuperMartijn642", L"https://modrinth.com/mod/connected-glass", false
    },
    {
        L"no-chat-reports", L"No Chat Reports", L"Безопасность", L"19.8M",
        L"Шифрование и отключение телеметрии криптографической подписи чата игрока.",
        L"Защищает аккаунт от глобальных банов и автоматических систем модерации чата серверов.",
        L"1.19.2 – 1.21.3", L"Fabric / Forge", L"Aizist", L"https://modrinth.com/mod/no-chat-reports", false
    }
};

// The live catalog deliberately starts empty. It is filled from Modrinth in small
// batches, so the first paint stays responsive and the list can continue forever.
static std::vector<ModItem> g_allMods;
static std::vector<ModItem> g_filteredMods;
static std::wstring g_modSearchQuery = L"";
static std::wstring g_selectedCategory = L"Все";
static const int MODS_PER_BATCH = 20;
static int g_modCatalogNextOffset = 0;
static int g_modCatalogTotalHits = -1;
static bool g_modCatalogHasMore = true;
static std::atomic<bool> g_modCatalogLoading(false);
static std::wstring g_modCatalogQuery;
static const ModItem* g_selectedModDetail = nullptr;
// This is a real view switch, not a guessed "installed" state in the online
// catalog. It always reads the active container's actual mods directory.
static bool g_showInstalledMods = false;

// A tab is a real vertical viewport now. Content stays comfortably sized; it is
// never compressed to squeeze into a short window.
static int g_tabScrollOffset = 0;

static std::vector<std::wstring> g_modCategories = {
    L"Все", L"Установленные", L"Оптимизация", L"Шейдеры", L"Интерфейс", L"Технологии", L"Библиотеки", L"Карты", L"Приключения", L"Декор", L"Магия", L"Мобы"
};

// Quick RAM presets (MB)
static std::vector<int> g_ramPresets = { 2048, 4096, 6144, 8192, 12288, 16384 };

// Forward declarations
void AppendLog(const std::wstring& line);
void StartMinecraftLaunch();
void FilterMods();
void SetActiveContainer(int index);
void SetGlobalRam(int mb);
void CreateNewContainerDialog();
void ApplyDwmDarkTitleBar(HWND hwnd);

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
// Persistent container profiles
// ----------------------------------------------------------------------------
// A profile name is not a cosmetic, transient value.  Earlier versions only
// wrote the global version/loader values to config.ini, so a renamed container
// silently reverted on the next refresh.  Keep the full profile list together
// with the active profile id in the same atomic Windows profile store.
static std::wstring GetConfigIniPath() {
    return g_config.storagePath + L"\\config.ini";
}

static void SaveContainerProfiles() {
    const std::wstring iniFile = GetConfigIniPath();
    EnsureDirectoryExists(g_config.storagePath);
    WritePrivateProfileStringW(L"Containers", L"Count", std::to_wstring(g_containers.size()).c_str(), iniFile.c_str());
    WritePrivateProfileStringW(L"Containers", L"ActiveId",
        (g_activeContainerIndex >= 0 && g_activeContainerIndex < (int)g_containers.size())
            ? g_containers[g_activeContainerIndex].id.c_str() : L"c-default",
        iniFile.c_str());

    for (size_t i = 0; i < g_containers.size(); ++i) {
        const std::wstring prefix = L"Container" + std::to_wstring(i);
        const ContainerProfile& profile = g_containers[i];
        WritePrivateProfileStringW(L"Containers", (prefix + L"Id").c_str(), profile.id.c_str(), iniFile.c_str());
        WritePrivateProfileStringW(L"Containers", (prefix + L"Name").c_str(), profile.name.c_str(), iniFile.c_str());
        WritePrivateProfileStringW(L"Containers", (prefix + L"Version").c_str(), profile.version.c_str(), iniFile.c_str());
        WritePrivateProfileStringW(L"Containers", (prefix + L"Loader").c_str(), profile.loader.c_str(), iniFile.c_str());
        WritePrivateProfileStringW(L"Containers", (prefix + L"MemoryMb").c_str(), std::to_wstring(profile.memoryMb).c_str(), iniFile.c_str());
    }
}

static void LoadContainerProfiles() {
    const std::wstring iniFile = GetConfigIniPath();
    const int count = GetPrivateProfileIntW(L"Containers", L"Count", 0, iniFile.c_str());
    if (count <= 0) {
        // First run / old config: preserve the one intentional default profile.
        g_containers[0].version = g_config.version;
        g_containers[0].loader = g_config.loader;
        g_containers[0].memoryMb = g_config.memoryMb;
        g_containers[0].name = L"Minecraft " + g_config.version + L" Vanilla";
        g_activeContainerIndex = 0;
        return;
    }

    std::vector<ContainerProfile> restored;
    WCHAR buf[512] = { 0 };
    for (int i = 0; i < count; ++i) {
        const std::wstring prefix = L"Container" + std::to_wstring(i);
        GetPrivateProfileStringW(L"Containers", (prefix + L"Id").c_str(), L"", buf, 512, iniFile.c_str());
        const std::wstring id = buf;
        GetPrivateProfileStringW(L"Containers", (prefix + L"Name").c_str(), L"", buf, 512, iniFile.c_str());
        const std::wstring name = buf;
        GetPrivateProfileStringW(L"Containers", (prefix + L"Version").c_str(), L"", buf, 512, iniFile.c_str());
        const std::wstring version = buf;
        GetPrivateProfileStringW(L"Containers", (prefix + L"Loader").c_str(), L"", buf, 512, iniFile.c_str());
        const std::wstring loader = buf;
        const int memory = GetPrivateProfileIntW(L"Containers", (prefix + L"MemoryMb").c_str(), g_config.memoryMb, iniFile.c_str());
        if (!id.empty() && !name.empty() && !version.empty() && !loader.empty()) {
            restored.push_back({ id, name, version, loader, memory });
        }
    }

    if (restored.empty()) return;
    g_containers = restored;

    GetPrivateProfileStringW(L"Containers", L"ActiveId", restored.front().id.c_str(), buf, 512, iniFile.c_str());
    const std::wstring activeId = buf;
    g_activeContainerIndex = 0;
    for (size_t i = 0; i < restored.size(); ++i) {
        if (restored[i].id == activeId) {
            g_activeContainerIndex = (int)i;
            break;
        }
    }
}

static std::vector<std::wstring> GetSettingsGcPresetLabels() {
    return {
        L"Aikar's Flags (FPS/TPS)",
        L"ZGC (Low-Latency)",
        L"Shenandoah GC",
        L"Standard G1GC",
        Tr(L"Свои флаги (Custom)", L"Custom Flags")
    };
}

static int GetSettingsGcPillWidth(const std::wstring& label) {
    return 24 + (int)label.length() * 7;
}

static int GetSettingsGcPillRows(int contentW) {
    const int right = contentW - 16;
    int x = 16;
    int rows = 1;
    for (const std::wstring& label : GetSettingsGcPresetLabels()) {
        const int width = GetSettingsGcPillWidth(label);
        if (x + width > right && x != 16) {
            x = 16;
            ++rows;
        }
        x += width + 6;
    }
    return rows;
}

static int GetSettingsJvmArgsLabelOffset(int contentW) {
    return 84 + (GetSettingsGcPillRows(contentW) - 1) * 30;
}

static int GetSettingsJvmArgsInputOffset(int contentW) {
    return GetSettingsJvmArgsLabelOffset(contentW) + 20;
}

static int GetSettingsJavaPathLabelOffset(int contentW) {
    return GetSettingsJvmArgsInputOffset(contentW) + 34;
}

static int GetSettingsJavaPathInputOffset(int contentW) {
    return GetSettingsJavaPathLabelOffset(contentW) + 16;
}

static int GetSettingsJvmCardHeight(int contentW) {
    return GetSettingsJavaPathInputOffset(contentW) + 36;
}

static int GetSettingsOptionsCardHeight(int contentW) {
    // On compact widths the toggles move to their own line rather than being
    // pushed beyond the right edge of the viewport.
    if (contentW >= 680) return 64;
    return contentW >= 390 ? 100 : 136;
}

static int GetSettingsThemePillsPerRow(int contentW) {
    const int usableWidth = std::max(114, contentW - 32);
    return std::max(1, (usableWidth + 8) / 122); // 114px pill + 8px gap
}

static int GetSettingsThemeCardHeight(int contentW) {
    const int rows = ((int)g_themes.size() + GetSettingsThemePillsPerRow(contentW) - 1) /
                     GetSettingsThemePillsPerRow(contentW);
    return 38 + rows * 34;
}

static int GetTabContentBottom() {
    const int contentY = 54 + 16;
    if (g_currentTab == 0) {
        const int cardY = contentY + 68;
        return cardY + (int)g_containers.size() * (120 + 16) + 16;
    }
    if (g_currentTab == 1) {
        if (g_selectedModDetail != nullptr) return contentY + 520;
        const int modListY = contentY + 58 + 52;
        const int itemCount = g_showInstalledMods && g_activeContainerIndex >= 0 &&
                g_activeContainerIndex < (int)g_containers.size()
            ? (int)GetInstalledModFiles(g_config.storagePath, g_containers[g_activeContainerIndex].id).size()
            : (int)g_filteredMods.size();
        // Cards are intentionally roomy: no compressed single-line rows.
        return modListY + itemCount * 80 + 50;
    }
    if (g_currentTab == 3) return contentY + 210;
    if (g_currentTab == 4) {
        // Width is unavailable here; reserve the tallest sensible wrapped
        // theme layout so the vertical scrollbar never clips compact windows.
        return contentY + 62 + 118 + GetSettingsJvmCardHeight(114) + 8 + 100 + 8 + 38 + (int)g_themes.size() * 34 + 16;
    }
    return contentY + 80;
}

static int GetTabScrollMax(int clientHeight) {
    const int visibleBottom = clientHeight - 84 - 10;
    return std::max(0, GetTabContentBottom() - visibleBottom);
}

static void ClampTabScroll(int clientHeight) {
    g_tabScrollOffset = std::max(0, std::min(g_tabScrollOffset, GetTabScrollMax(clientHeight)));
}

// ----------------------------------------------------------------------------
// Modrinth Download & File Management Helpers
// ----------------------------------------------------------------------------
inline std::string FetchHttpString(const std::wstring& url) {
    HINTERNET hInternet = InternetOpenW(L"Sqikpeek-Aether/1.0.38 (admin@sqikpeek.com)", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return "";
    const wchar_t* headers = L"User-Agent: Sqikpeek-Aether/1.0.38 (admin@sqikpeek.com)\r\nAccept: application/json\r\n";
    HINTERNET hUrl = InternetOpenUrlW(hInternet, url.c_str(), headers, (DWORD)wcslen(headers),
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE | INTERNET_FLAG_SECURE, 0);
    if (!hUrl) {
        InternetCloseHandle(hInternet);
        return "";
    }
    std::string result;
    char buffer[8192];
    DWORD bytesRead = 0;
    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        result.append(buffer, bytesRead);
        if (result.size() > 5 * 1024 * 1024) break;
    }
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);
    return result;
}

struct ModrinthDependencyRef {
    std::string versionId;
    std::string projectId;
};

struct ModrinthVersionInfo {
    std::string id;
    std::string projectId;
    std::wstring fileUrl;
    std::wstring filename;
    std::vector<std::string> gameVersions;
    std::vector<std::string> loaders;
    std::vector<ModrinthDependencyRef> requiredDependencies;
};

static std::string GetJsonStringField(const std::string& json, const char* key) {
    const std::string marker = std::string("\"") + key + "\"";
    const size_t keyPos = json.find(marker);
    if (keyPos == std::string::npos) return "";
    const size_t colon = json.find(':', keyPos + marker.size());
    if (colon == std::string::npos) return "";
    const size_t q1 = json.find('"', colon + 1);
    if (q1 == std::string::npos) return "";

    std::string value;
    bool escaped = false;
    for (size_t i = q1 + 1; i < json.size(); ++i) {
        const char c = json[i];
        if (escaped) {
            value += c;
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else if (c == '"') {
            return value;
        } else {
            value += c;
        }
    }
    return "";
}

static bool ExtractJsonObject(const std::string& json, size_t startAt, std::string& object, size_t* after = nullptr) {
    const size_t begin = json.find('{', startAt);
    if (begin == std::string::npos) return false;

    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (size_t i = begin; i < json.size(); ++i) {
        const char c = json[i];
        if (inString) {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') inString = false;
            continue;
        }
        if (c == '"') inString = true;
        else if (c == '{') ++depth;
        else if (c == '}' && --depth == 0) {
            object = json.substr(begin, i - begin + 1);
            if (after) *after = i + 1;
            return true;
        }
    }
    return false;
}

static std::string GetJsonArrayField(const std::string& json, const char* key) {
    const std::string marker = std::string("\"") + key + "\"";
    const size_t keyPos = json.find(marker);
    if (keyPos == std::string::npos) return "";
    const size_t open = json.find('[', keyPos + marker.size());
    if (open == std::string::npos) return "";

    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (size_t i = open; i < json.size(); ++i) {
        const char c = json[i];
        if (inString) {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') inString = false;
            continue;
        }
        if (c == '"') inString = true;
        else if (c == '[') ++depth;
        else if (c == ']' && --depth == 0) return json.substr(open + 1, i - open - 1);
    }
    return "";
}

static std::vector<std::string> GetJsonStringValues(const std::string& arrayJson) {
    std::vector<std::string> values;
    bool inString = false;
    bool escaped = false;
    std::string value;
    for (char c : arrayJson) {
        if (!inString) {
            if (c == '"') { inString = true; value.clear(); }
        } else if (escaped) {
            value += c;
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else if (c == '"') {
            values.push_back(value);
            inString = false;
        } else {
            value += c;
        }
    }
    return values;
}

static bool ModrinthVersionMatchesContainer(const ModrinthVersionInfo& version,
                                            const std::wstring& mcVersion,
                                            const std::wstring& loader) {
    const std::string requestedVersion(mcVersion.begin(), mcVersion.end());
    const std::wstring normalizedLoader = ToLowerW(loader);
    const bool versionMatches = std::find(version.gameVersions.begin(), version.gameVersions.end(), requestedVersion) !=
                                version.gameVersions.end();
    bool loaderMatches = false;
    for (const std::string& item : version.loaders) {
        std::wstring value(item.begin(), item.end());
        if (ToLowerW(value) == normalizedLoader) {
            loaderMatches = true;
            break;
        }
    }
    return versionMatches && loaderMatches;
}

static bool ParseModrinthVersion(const std::string& versionJson, ModrinthVersionInfo& version) {
    version = {};
    std::string object = versionJson;
    // /project/{id}/version returns an array, while /version/{id} returns an object.
    if (!object.empty() && object.front() == '[') {
        if (!ExtractJsonObject(object, 0, object)) return false;
    }

    version.id = GetJsonStringField(object, "id");
    version.projectId = GetJsonStringField(object, "project_id");
    version.gameVersions = GetJsonStringValues(GetJsonArrayField(object, "game_versions"));
    version.loaders = GetJsonStringValues(GetJsonArrayField(object, "loaders"));
    const std::string files = GetJsonArrayField(object, "files");
    if (files.empty()) return false;

    std::string firstFile;
    std::string chosenFile;
    size_t cursor = 0;
    while (cursor < files.size()) {
        std::string file;
        size_t after = 0;
        if (!ExtractJsonObject(files, cursor, file, &after)) break;
        if (firstFile.empty()) firstFile = file;
        if (file.find("\"primary\":true") != std::string::npos ||
            file.find("\"primary\" : true") != std::string::npos) {
            chosenFile = file;
            break;
        }
        cursor = after;
    }
    if (chosenFile.empty()) chosenFile = firstFile;
    if (chosenFile.empty()) return false;

    const std::string url = GetJsonStringField(chosenFile, "url");
    const std::string filename = GetJsonStringField(chosenFile, "filename");
    if (url.empty() || filename.empty()) return false;
    version.fileUrl.assign(url.begin(), url.end());
    version.filename.assign(filename.begin(), filename.end());

    const std::string dependencies = GetJsonArrayField(object, "dependencies");
    cursor = 0;
    while (!dependencies.empty() && cursor < dependencies.size()) {
        std::string dependency;
        size_t after = 0;
        if (!ExtractJsonObject(dependencies, cursor, dependency, &after)) break;
        cursor = after;
        if (GetJsonStringField(dependency, "dependency_type") != "required") continue;

        ModrinthDependencyRef ref;
        ref.versionId = GetJsonStringField(dependency, "version_id");
        ref.projectId = GetJsonStringField(dependency, "project_id");
        if (!ref.versionId.empty() || !ref.projectId.empty()) version.requiredDependencies.push_back(ref);
    }
    return true;
}

static std::wstring NormalizeModrinthIdentifier(const std::wstring& value) {
    std::wstring result = ToLowerW(value);
    const size_t urlMarker = result.find(L"/mod/");
    if (urlMarker != std::wstring::npos) result = result.substr(urlMarker + 5);
    const size_t slash = result.find(L'/');
    if (slash != std::wstring::npos) result = result.substr(0, slash);
    while (!result.empty() && (result.back() == L' ' || result.back() == L'/')) result.pop_back();
    while (!result.empty() && result.front() == L' ') result.erase(result.begin());
    for (auto& c : result) if (c == L' ') c = L'-';
    return result;
}

static bool FetchCompatibleModrinthVersion(
    const std::wstring& projectIdOrSlug,
    const std::wstring& mcVersion,
    const std::wstring& loader,
    ModrinthVersionInfo& version
) {
    const std::wstring cleanLoader = ToLowerW(loader);
    // A mod jar cannot run in a vanilla instance. Do not quietly substitute
    // Fabric: that was the source of wrong-version/wrong-loader downloads.
    if (cleanLoader.empty() || cleanLoader == L"vanilla") return false;

    const std::wstring id = NormalizeModrinthIdentifier(projectIdOrSlug);
    if (id.empty()) return false;
    const std::wstring url = L"https://api.modrinth.com/v2/project/" + id +
        L"/version?loaders=%5B%22" + cleanLoader + L"%22%5D&game_versions=%5B%22" + mcVersion + L"%22%5D";
    return ParseModrinthVersion(FetchHttpString(url), version);
}

static bool ResolveCompatibleModrinthVersion(
    const std::wstring& slugOrName,
    const std::wstring& mcVersion,
    const std::wstring& loader,
    ModrinthVersionInfo& version
) {
    if (FetchCompatibleModrinthVersion(slugOrName, mcVersion, loader, version)) return true;

    // A catalog card normally already contains a slug. Search only resolves a
    // human-readable title; the selected version/loader filter remains strict.
    const std::wstring query = NormalizeModrinthIdentifier(slugOrName);
    const std::string searchJson = FetchHttpString(
        L"https://api.modrinth.com/v2/search?query=" + query + L"&limit=1&facets=%5B%5B%22project_type%3Amod%22%5D%5D"
    );
    const std::string slug = GetJsonStringField(searchJson, "slug");
    if (slug.empty()) return false;
    return FetchCompatibleModrinthVersion(std::wstring(slug.begin(), slug.end()), mcVersion, loader, version);
}

static bool DownloadVersionAndDependencies(
    const ModrinthVersionInfo& version,
    const std::wstring& containerModsDir,
    const std::wstring& mcVersion,
    const std::wstring& loader,
    std::set<std::string>& visitedVersions,
    std::function<void(int pct)> progress,
    bool reportProgress
) {
    const std::string key = !version.id.empty() ? version.id : version.projectId + "|" + std::string(version.filename.begin(), version.filename.end());
    if (!visitedVersions.insert(key).second) return true;

    // Dependencies are installed first. Each required dependency receives the
    // exact version specified by Modrinth, or an exact game+loader filtered
    // version if Modrinth supplied only a project id.
    for (const ModrinthDependencyRef& dependency : version.requiredDependencies) {
        ModrinthVersionInfo dependencyVersion;
        bool resolved = false;
        if (!dependency.versionId.empty()) {
            resolved = ParseModrinthVersion(FetchHttpString(
                L"https://api.modrinth.com/v2/version/" + std::wstring(dependency.versionId.begin(), dependency.versionId.end())
            ), dependencyVersion);
        } else if (!dependency.projectId.empty()) {
            resolved = FetchCompatibleModrinthVersion(
                std::wstring(dependency.projectId.begin(), dependency.projectId.end()), mcVersion, loader, dependencyVersion
            );
        }
        // Modrinth may point at a version id directly. Validate that exact
        // dependency too; never install a jar for a different MC version or
        // loader merely because its parent declared it.
        if (!resolved || !ModrinthVersionMatchesContainer(dependencyVersion, mcVersion, loader) ||
            !DownloadVersionAndDependencies(dependencyVersion, containerModsDir, mcVersion, loader,
                                            visitedVersions, progress, false)) {
            return false;
        }
    }

    const std::wstring destination = containerModsDir + L"\\\\" + version.filename;
    HANDLE existing = CreateFileW(destination.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (existing != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER size;
        GetFileSizeEx(existing, &size);
        CloseHandle(existing);
        if (size.QuadPart >= 500) return true;
    }

    const bool downloaded = MinecraftLauncher::DownloadHttpFile(
        version.fileUrl, destination, reportProgress ? progress : std::function<void(int pct)>()
    );
    if (!downloaded) {
        DeleteFileW(destination.c_str());
        return false;
    }

    HANDLE downloadedFile = CreateFileW(destination.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (downloadedFile == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER fileSize;
    GetFileSizeEx(downloadedFile, &fileSize);
    CloseHandle(downloadedFile);
    if (fileSize.QuadPart < 500) {
        DeleteFileW(destination.c_str());
        return false;
    }
    return true;
}

inline bool DownloadModrinthMod(
    const std::wstring& slugOrId,
    const std::wstring& modName,
    const std::wstring& containerModsDir,
    const std::wstring& mcVersion,
    const std::wstring& loader,
    std::function<void(int pct)> progress
) {
    EnsureDirectoryExists(containerModsDir);

    ModrinthVersionInfo rootVersion;
    if (!ResolveCompatibleModrinthVersion(slugOrId, mcVersion, loader, rootVersion)) {
        AppendLog(Tr(L"[Modrinth] Для «" + modName + L"» нет версии для Minecraft " + mcVersion + L" и загрузчика " + loader + L".",
                     L"[Modrinth] No compatible " + mcVersion + L" / " + loader + L" version exists for «" + modName + L"»."));
        return false;
    }

    if (!ModrinthVersionMatchesContainer(rootVersion, mcVersion, loader)) {
        AppendLog(Tr(L"[Modrinth] API вернул несовместимую версию для активного контейнера.",
                     L"[Modrinth] The API returned a version incompatible with the active container."));
        return false;
    }

    std::set<std::string> visitedVersions;
    const bool ok = DownloadVersionAndDependencies(rootVersion, containerModsDir, mcVersion, loader,
                                                    visitedVersions, progress, true);
    if (ok && visitedVersions.size() > 1) {
        AppendLog(Tr(L"[Modrinth] Автоматически установлены обязательные зависимости: " +
                     std::to_wstring(visitedVersions.size() - 1),
                     L"[Modrinth] Required dependencies installed automatically: " +
                     std::to_wstring(visitedVersions.size() - 1)));
    }
    return ok;
}

inline bool RemoveModrinthMod(const std::wstring& slugOrId, const std::wstring& storageDir, const std::wstring& mcVersion) {
    std::wstring modsDir = storageDir + L"\\instances\\" + mcVersion + L"\\mods";
    std::wstring pattern = modsDir + L"\\*" + slugOrId + L"*.jar";
    WIN32_FIND_DATAW wfd;
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &wfd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            DeleteFileW((modsDir + L"\\" + wfd.cFileName).c_str());
        } while (FindNextFileW(hFind, &wfd));
        FindClose(hFind);
    }

    WCHAR appData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appData))) {
        std::wstring mcModsDir = std::wstring(appData) + L"\\.minecraft\\mods";
        std::wstring mcPattern = mcModsDir + L"\\*" + slugOrId + L"*.jar";
        HANDLE hMcFind = FindFirstFileW(mcPattern.c_str(), &wfd);
        if (hMcFind != INVALID_HANDLE_VALUE) {
            do {
                DeleteFileW((mcModsDir + L"\\" + wfd.cFileName).c_str());
            } while (FindNextFileW(hMcFind, &wfd));
            FindClose(hMcFind);
        }
    }
    return true;
}

inline void SetModState(const std::wstring& slugOrName, bool installed, bool downloading, int pct) {
    for (auto& m : g_allMods) {
        if (ToLowerW(m.id) == ToLowerW(slugOrName) || ToLowerW(m.name) == ToLowerW(slugOrName)) {
            m.installed = installed;
            m.isDownloading = downloading;
            m.downloadProgress = pct;
        }
    }
    for (auto& m : g_filteredMods) {
        if (ToLowerW(m.id) == ToLowerW(slugOrName) || ToLowerW(m.name) == ToLowerW(slugOrName)) {
            m.installed = installed;
            m.isDownloading = downloading;
            m.downloadProgress = pct;
        }
    }
}


static std::wstring UrlEncodeQuery(const std::wstring& value) {
    if (value.empty()) return L"";
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), (int)value.size(), NULL, 0, NULL, NULL);
    std::string utf8(bytes, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), (int)value.size(), utf8.data(), bytes, NULL, NULL);

    std::wstringstream encoded;
    encoded << std::uppercase << std::hex;
    for (unsigned char c : utf8) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.') {
            encoded << (wchar_t)c;
        } else {
            encoded << L'%' << std::setw(2) << std::setfill(L'0') << (int)c;
        }
    }
    return encoded.str();
}

static int GetJsonIntegerField(const std::string& json, const char* key, int fallback = -1) {
    const std::string marker = std::string("\"") + key + "\"";
    const size_t keyPos = json.find(marker);
    if (keyPos == std::string::npos) return fallback;
    const size_t colon = json.find(':', keyPos + marker.size());
    if (colon == std::string::npos) return fallback;
    return atoi(json.c_str() + colon + 1);
}

static std::atomic<unsigned long> g_modCatalogGeneration(0);
static constexpr UINT WM_AETHER_MODRINTH_BATCH = WM_APP + 42;

// HTTP work happens away from the Win32 paint thread. Results are delivered back
// to the window as one complete batch, so PaintMainUI never races a vector
// reallocation while it is drawing cards.
struct ModrinthCatalogBatch {
    unsigned long generation = 0;
    int offset = 0;
    int totalHits = -1;
    bool requestFailed = false;
    std::vector<ModItem> mods;
};

static void FetchModrinthOnlineMods(
    const std::wstring& query,
    int offset,
    const std::wstring& loader,
    const std::wstring& mcVersion,
    HWND hwnd
) {
    if (g_modCatalogLoading.exchange(true)) return;
    const unsigned long generation = g_modCatalogGeneration.load();

    std::thread([query, offset, loader, mcVersion, hwnd, generation]() {
        auto* batch = new ModrinthCatalogBatch();
        batch->generation = generation;
        batch->offset = offset;

        std::wstring apiUrl = L"https://api.modrinth.com/v2/search?limit=" + std::to_wstring(MODS_PER_BATCH) +
            L"&offset=" + std::to_wstring(offset) +
            L"&facets=%5B%5B%22project_type%3Amod%22%5D%2C%5B%22versions%3A" + mcVersion + L"%22%5D";
        if (!loader.empty() && ToLowerW(loader) != L"vanilla") {
            apiUrl += L"%2C%5B%22categories%3A" + UrlEncodeQuery(ToLowerW(loader)) + L"%22%5D";
        }
        apiUrl += L"%5D";
        if (!query.empty()) apiUrl += L"&query=" + UrlEncodeQuery(query);

        const std::string json = FetchHttpString(apiUrl);
        if (json.empty()) {
            batch->requestFailed = true;
        } else {
            const std::string hits = GetJsonArrayField(json, "hits");
            batch->totalHits = GetJsonIntegerField(json, "total_hits", -1);
            size_t cursor = 0;
            while (cursor < hits.size()) {
                std::string hit;
                size_t after = 0;
                if (!ExtractJsonObject(hits, cursor, hit, &after)) break;
                cursor = after;

                const std::string slug = GetJsonStringField(hit, "slug");
                if (slug.empty()) continue;
                const std::string title = GetJsonStringField(hit, "title");
                const std::string description = GetJsonStringField(hit, "description");
                const std::string author = GetJsonStringField(hit, "author");
                const int downloads = GetJsonIntegerField(hit, "downloads", 0);
                std::wstring wDownloads;
                if (downloads >= 1000000) wDownloads = std::to_wstring(downloads / 1000000) + L"M";
                else if (downloads >= 1000) wDownloads = std::to_wstring(downloads / 1000) + L"K";
                else wDownloads = std::to_wstring(std::max(0, downloads));

                ModItem mod;
                mod.id.assign(slug.begin(), slug.end());
                mod.name = title.empty() ? mod.id : std::wstring(title.begin(), title.end());
                mod.category = L"Modrinth";
                mod.downloads = wDownloads;
                mod.fullDesc.assign(description.begin(), description.end());
                mod.brief = mod.fullDesc.substr(0, std::min<size_t>(150, mod.fullDesc.size()));
                mod.versions = mcVersion;
                mod.loader = (loader.empty() || ToLowerW(loader) == L"vanilla") ? L"Любой загрузчик" : loader;
                mod.author = author.empty() ? L"Modrinth" : std::wstring(author.begin(), author.end());
                mod.url = L"https://modrinth.com/mod/" + mod.id;
                // This deliberately stays false until the UI thread synchronizes
                // the active container's real mods directory.
                mod.installed = false;
                batch->mods.push_back(std::move(mod));
            }
        }

        // A reset may have happened during the request. The message handler
        // checks the generation once more before it changes visible state.
        if (!hwnd || !PostMessageW(hwnd, WM_AETHER_MODRINTH_BATCH, 0, reinterpret_cast<LPARAM>(batch))) {
            delete batch;
            if (generation == g_modCatalogGeneration.load()) g_modCatalogLoading.store(false);
        }
    }).detach();
}

static void ApplyModrinthCatalogBatch(ModrinthCatalogBatch* batch, HWND hwnd) {
    std::unique_ptr<ModrinthCatalogBatch> owned(batch);
    if (!batch || batch->generation != g_modCatalogGeneration.load()) return;

    int added = 0;
    if (!batch->requestFailed) {
        for (const ModItem& mod : batch->mods) {
            bool exists = false;
            for (const ModItem& existing : g_allMods) {
                if (ToLowerW(existing.id) == ToLowerW(mod.id)) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                g_allMods.push_back(mod);
                ++added;
            }
        }
        g_modCatalogTotalHits = batch->totalHits;
        g_modCatalogNextOffset = batch->offset + MODS_PER_BATCH;
        g_modCatalogHasMore = !batch->mods.empty() &&
            (batch->totalHits < 0 || g_modCatalogNextOffset < batch->totalHits);
    } else {
        g_modCatalogHasMore = false;
        AppendLog(Tr(L"[Modrinth] [ОШИБКА] Каталог не ответил. Проверьте подключение и повторите поиск.",
                     L"[Modrinth] [ERROR] The catalog did not respond. Check your connection and search again."));
    }
    g_modCatalogLoading.store(false);

    // Installed labels are only derived from files currently in this container.
    if (g_activeContainerIndex >= 0 && g_activeContainerIndex < (int)g_containers.size()) {
        SyncInstalledModsWithDisk(g_config.storagePath, g_containers[g_activeContainerIndex].id);
    } else {
        FilterMods();
    }
    if (added > 0) {
        AppendLog(Tr(L"[Modrinth] Загружено " + std::to_wstring(added) + L" модов. Прокрутите вниз для следующей партии.",
                     L"[Modrinth] Loaded " + std::to_wstring(added) + L" mods. Scroll down for the next batch."));
    }
    if (hwnd) InvalidateRect(hwnd, NULL, FALSE);
}

static void ResetModrinthCatalog(const std::wstring& query, HWND hwnd) {
    ++g_modCatalogGeneration;
    g_modCatalogLoading.store(false);
    g_modCatalogQuery = query;
    g_modCatalogNextOffset = 0;
    g_modCatalogTotalHits = -1;
    g_modCatalogHasMore = true;
    g_allMods.clear();
    g_filteredMods.clear();
    g_tabScrollOffset = 0;
    FetchModrinthOnlineMods(query, 0, g_config.loader, g_activeVersion, hwnd);
}

static void FetchNextModrinthCatalogBatch(HWND hwnd) {
    if (!g_modCatalogHasMore || g_modCatalogLoading.load()) return;
    FetchModrinthOnlineMods(g_modCatalogQuery, g_modCatalogNextOffset, g_config.loader, g_activeVersion, hwnd);
}

inline void TriggerModDownload(const std::wstring& slug, const std::wstring& modTitle, HWND hwnd) {
    SetModState(slug, false, true, 10);
    AppendLog(Tr(L"[Modrinth] Начат поиск и скачивание мода: " + modTitle,
                 L"[Modrinth] Started searching and downloading mod: " + modTitle));
    InvalidateRect(hwnd, NULL, FALSE);

    std::wstring storageDir = g_config.storagePath;
    std::wstring ver = g_activeVersion;
    std::wstring loader = g_config.loader;
    std::wstring containerId = g_containers[g_activeContainerIndex].id;
    std::wstring containerModsDir = GetContainerModsDir(storageDir, containerId);

    std::thread([slug, modTitle, storageDir, containerId, containerModsDir, ver, loader, hwnd]() {
        bool ok = DownloadModrinthMod(slug, modTitle, containerModsDir, ver, loader, [slug, hwnd](int pct) {
            SetModState(slug, false, true, pct);
            InvalidateRect(hwnd, NULL, FALSE);
        });

        if (ok) {
            SetModState(slug, true, false, 100);
            SyncInstalledModsWithDisk(storageDir, containerId);
            AppendLog(Tr(L"[Modrinth] Мод «" + modTitle + L"» успешно загружен и сохранен в папку mods контейнера!",
                         L"[Modrinth] Mod «" + modTitle + L"» successfully saved to container mods folder!"));
        } else {
            SetModState(slug, false, false, 0);
            AppendLog(Tr(L"[Modrinth] [ОШИБКА] Не удалось скачать «" + modTitle + L"»! Проверьте соединение или доступ к Modrinth.",
                         L"[Modrinth] [ERROR] Failed to download «" + modTitle + L"»! Check network or Modrinth access."));
        }
        InvalidateRect(hwnd, NULL, FALSE);
    }).detach();
}

inline void TriggerModRemove(const std::wstring& slug, const std::wstring& modTitle, HWND hwnd) {
    std::wstring storageDir = g_config.storagePath;
    std::wstring containerId = g_containers[g_activeContainerIndex].id;
    std::wstring containerModsDir = GetContainerModsDir(storageDir, containerId);

    std::vector<std::wstring> jars = GetInstalledModFiles(storageDir, containerId);
    std::wstring lSlug = ToLowerW(slug);
    std::wstring lTitle = ToLowerW(modTitle);

    for (const auto& jar : jars) {
        std::wstring lJar = ToLowerW(jar);
        if (lJar.find(lSlug) != std::wstring::npos || lJar.find(lTitle) != std::wstring::npos) {
            std::wstring fullPath = containerModsDir + L"\\" + jar;
            DeleteFileW(fullPath.c_str());
        }
    }

    SetModState(slug, false, false, 0);
    SyncInstalledModsWithDisk(storageDir, containerId);
    AppendLog(Tr(L"[Modrinth] Мод «" + modTitle + L"» удален из папки контейнера.",
                 L"[Modrinth] Mod «" + modTitle + L"» removed from container folder."));
    InvalidateRect(hwnd, NULL, FALSE);
}

// ----------------------------------------------------------------------------
// DWM Immersive Dark Title Bar (Eliminates the Stark White Title Bar Border!)
// ----------------------------------------------------------------------------
void ApplyDwmDarkTitleBar(HWND hwnd) {
    if (!hwnd) return;
    BOOL darkMode = TRUE;
    // DWMWA_USE_IMMERSIVE_DARK_MODE (20 on Win11 / Win10 20H1+, 19 on older Win10)
    DwmSetWindowAttribute(hwnd, 20, &darkMode, sizeof(darkMode));
    DwmSetWindowAttribute(hwnd, 19, &darkMode, sizeof(darkMode));

    // Match titlebar caption to active dark theme surface color (Win11 DWMWA_CAPTION_COLOR 35)
    COLORREF captionColor = RGB(g_currentTheme.bgSurface.GetR(), g_currentTheme.bgSurface.GetG(), g_currentTheme.bgSurface.GetB());
    DwmSetWindowAttribute(hwnd, 35, &captionColor, sizeof(captionColor));

    COLORREF textColor = RGB(240, 242, 248);
    DwmSetWindowAttribute(hwnd, 36 /* DWMWA_TEXT_COLOR */, &textColor, sizeof(textColor));
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
// Container Selection & Global RAM
// ----------------------------------------------------------------------------
void SetGlobalRam(int mb) {
    g_config.memoryMb = mb;
    for (auto& c : g_containers) {
        c.memoryMb = mb;
    }
    SaveConfig(g_config);
    SaveContainerProfiles();
}

void SetActiveContainer(int index) {
    if (index >= 0 && index < (int)g_containers.size()) {
        g_activeContainerIndex = index;
        g_activeContainerName = g_containers[index].name;
        g_activeVersion = g_containers[index].version;
        g_config.version = g_containers[index].version;
        g_config.loader = g_containers[index].loader;
        if (g_config.loader == L"fabric") g_selectedLoaderFilter = L"Fabric";
        else if (g_config.loader == L"forge") g_selectedLoaderFilter = L"Forge";
        else if (g_config.loader == L"neoforge") g_selectedLoaderFilter = L"NeoForge";
        else if (g_config.loader == L"quilt") g_selectedLoaderFilter = L"Quilt";
        else g_selectedLoaderFilter = L"Все";
        EnsureContainerStructure(g_config.storagePath, g_containers[index].id);
        SyncInstalledModsWithDisk(g_config.storagePath, g_containers[index].id);
        SaveConfig(g_config);
        SaveContainerProfiles();
        FilterMods();
        if (g_hMainWindow) ResetModrinthCatalog(g_modSearchQuery, g_hMainWindow);

        AppendLog(Tr(L"[Aether] Выбран активный контейнер: " + g_activeContainerName + L" (" + g_config.loader + L")",
                     L"[Aether] Selected active container: " + g_activeContainerName));

        if (g_hMainWindow) {
            InvalidateRect(g_hMainWindow, NULL, FALSE);
        }
    }
}

void CreateNewContainerDialog() {
    HMENU hVerMenu = CreatePopupMenu();
    AppendMenuW(hVerMenu, MF_STRING, 4001, L"Minecraft 1.21.1 (Vanilla)");
    AppendMenuW(hVerMenu, MF_STRING, 4002, L"Minecraft 1.21.1 (Fabric)");
    AppendMenuW(hVerMenu, MF_STRING, 4003, L"Minecraft 1.20.4 (Fabric)");
    AppendMenuW(hVerMenu, MF_STRING, 4004, L"Minecraft 1.20.1 (Forge)");
    AppendMenuW(hVerMenu, MF_STRING, 4005, L"Minecraft 1.20.1 (Fabric)");
    AppendMenuW(hVerMenu, MF_STRING, 4006, L"Minecraft 1.16.5 (Forge)");
    AppendMenuW(hVerMenu, MF_STRING, 4007, L"Minecraft 1.12.2 (Forge)");

    POINT pt = { 220 + 24 + 200, 54 + 20 + 34 };
    ClientToScreen(g_hMainWindow, &pt);
    int cmd = TrackPopupMenuEx(hVerMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD, pt.x, pt.y, g_hMainWindow, NULL);
    DestroyMenu(hVerMenu);

    std::wstring newVer = L"1.21.1";
    std::wstring newLdr = L"vanilla";
    std::wstring newName = L"Minecraft 1.21.1 Vanilla";

    if (cmd == 4001) { newVer = L"1.21.1"; newLdr = L"vanilla"; newName = L"Minecraft 1.21.1 Vanilla"; }
    else if (cmd == 4002) { newVer = L"1.21.1"; newLdr = L"fabric"; newName = L"Fabric 1.21.1 Performance"; }
    else if (cmd == 4003) { newVer = L"1.20.4"; newLdr = L"fabric"; newName = L"Fabric 1.20.4 Shaders"; }
    else if (cmd == 4004) { newVer = L"1.20.1"; newLdr = L"forge"; newName = L"Forge 1.20.1 Tech & Magic"; }
    else if (cmd == 4005) { newVer = L"1.20.1"; newLdr = L"fabric"; newName = L"Fabric 1.20.1 Modded"; }
    else if (cmd == 4006) { newVer = L"1.16.5"; newLdr = L"forge"; newName = L"Forge 1.16.5 Classic"; }
    else if (cmd == 4007) { newVer = L"1.12.2"; newLdr = L"forge"; newName = L"Forge 1.12.2 Golden Era"; }
    else return;

    std::wstring newId = L"c-" + std::to_wstring(g_containers.size() + 1);
    EnsureContainerStructure(g_config.storagePath, newId);
    g_containers.push_back({ newId, newName, newVer, newLdr, g_config.memoryMb });
    SetActiveContainer((int)g_containers.size() - 1);
    SaveContainerProfiles();
    AppendLog(Tr(L"[Aether] Создан новый контейнер: " + newName, L"[Aether] Created new container: " + newName));
}

// ----------------------------------------------------------------------------
// Mod Filter Logic
// ----------------------------------------------------------------------------
void FilterMods() {
    g_filteredMods.clear();
    std::wstring q = ToLowerW(g_modSearchQuery);
    std::wstring lFilter = ToLowerW(g_selectedLoaderFilter);

    for (const auto& m : g_allMods) {
        // "Installed" is intentionally not a catalog category. That view is
        // rendered directly from the active container's mods directory, so a
        // stale category can never hide successfully fetched Modrinth cards.
        if (g_selectedCategory != L"Все" && g_selectedCategory != L"Установленные" &&
            m.category != g_selectedCategory) {
            continue;
        }

        // Loader filter
        if (lFilter != L"все" && !lFilter.empty()) {
            std::wstring mLoader = ToLowerW(m.loader);
            if (mLoader.find(L"все") == std::wstring::npos &&
                mLoader.find(lFilter) == std::wstring::npos) {
                if (lFilter == L"quilt" && mLoader.find(L"fabric") != std::wstring::npos) {
                    // Quilt is compatible with Fabric mods
                } else {
                    continue;
                }
            }
        }

        // Search text filter
        if (!q.empty()) {
            if (ToLowerW(m.name).find(q) == std::wstring::npos &&
                ToLowerW(m.brief).find(q) == std::wstring::npos &&
                ToLowerW(m.fullDesc).find(q) == std::wstring::npos &&
                ToLowerW(m.category).find(q) == std::wstring::npos &&
                ToLowerW(m.loader).find(q) == std::wstring::npos) {
                continue;
            }
        }

        g_filteredMods.push_back(m);
    }
}


static std::vector<std::wstring> GetInstalledModFiles(const std::wstring& storageDir, const std::wstring& containerId) {
    std::vector<std::wstring> result;
    EnsureContainerStructure(storageDir, containerId);
    std::wstring modsDir = GetContainerModsDir(storageDir, containerId);
    std::wstring pattern = modsDir + L"\\*.jar";
    WIN32_FIND_DATAW wfd;
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &wfd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                result.push_back(wfd.cFileName);
            }
        } while (FindNextFileW(hFind, &wfd));
        FindClose(hFind);
    }
    return result;
}

static void SyncInstalledModsWithDisk(const std::wstring& storageDir, const std::wstring& containerId) {
    std::vector<std::wstring> jars = GetInstalledModFiles(storageDir, containerId);
    for (auto& m : g_allMods) {
        m.installed = false;
        std::wstring mId = ToLowerW(m.id);
        std::wstring mName = ToLowerW(m.name);
        for (const auto& jar : jars) {
            std::wstring lJar = ToLowerW(jar);
            if (lJar.find(mId) != std::wstring::npos || lJar.find(mName) != std::wstring::npos) {
                m.installed = true;
                break;
            }
        }
    }
    FilterMods();
}


bool PromptTextInput(HWND hParent, const std::wstring& title, const std::wstring& prompt, const std::wstring& initialVal, std::wstring& outVal) {
    bool okClicked = false;
    outVal = initialVal;

    static bool classRegistered = false;
    const wchar_t* dlgClassName = L"AetherInputPromptClass";
    if (!classRegistered) {
        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = g_hInstance;
        wc.lpszClassName = dlgClassName;
        wc.hbrBackground = CreateSolidBrush(RGB(24, 25, 32));
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        RegisterClassExW(&wc);
        classRegistered = true;
    }

    RECT prc;
    GetWindowRect(hParent, &prc);
    int dlgW = 440;
    int dlgH = 170;
    int dlgX = prc.left + (prc.right - prc.left - dlgW) / 2;
    int dlgY = prc.top + (prc.bottom - prc.top - dlgH) / 2;

    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        dlgClassName,
        title.c_str(),
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        dlgX, dlgY, dlgW, dlgH,
        hParent, NULL, g_hInstance, NULL
    );

    if (!hDlg) return false;

    ApplyDwmDarkTitleBar(hDlg);

    HFONT hFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT hFontBold = CreateFontW(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    HWND hStatic = CreateWindowExW(
        0, L"STATIC", prompt.c_str(),
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 16, dlgW - 40, 24,
        hDlg, NULL, g_hInstance, NULL
    );
    SendMessageW(hStatic, WM_SETFONT, (WPARAM)hFont, TRUE);

    HWND hEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", initialVal.c_str(),
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        20, 46, dlgW - 55, 26,
        hDlg, (HMENU)201, g_hInstance, NULL
    );
    SendMessageW(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessageW(hEdit, EM_SETSEL, 0, -1);
    SetFocus(hEdit);

    HWND hBtnOk = CreateWindowExW(
        0, L"BUTTON", Tr(L"OK", L"OK"),
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        dlgW - 205, 88, 80, 28,
        hDlg, (HMENU)IDOK, g_hInstance, NULL
    );
    SendMessageW(hBtnOk, WM_SETFONT, (WPARAM)hFontBold, TRUE);

    HWND hBtnCancel = CreateWindowExW(
        0, L"BUTTON", Tr(L"Отмена", L"Cancel"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        dlgW - 115, 88, 80, 28,
        hDlg, (HMENU)IDCANCEL, g_hInstance, NULL
    );
    SendMessageW(hBtnCancel, WM_SETFONT, (WPARAM)hFont, TRUE);

    EnableWindow(hParent, FALSE);

    MSG msg;
    bool running = true;
    while (running && GetMessageW(&msg, NULL, 0, 0) > 0) {
        // A popup close button arrives as WM_SYSCOMMAND/SC_CLOSE, not as the
        // WM_CLOSE message this former nested loop waited for. Handle both so
        // closing a rename prompt can never leave its disabled parent hanging.
        if (msg.hwnd == hDlg &&
            ((msg.message == WM_SYSCOMMAND && (msg.wParam & 0xFFF0) == SC_CLOSE) ||
             msg.message == WM_CLOSE)) {
            running = false;
            continue;
        }

        const bool belongsToPrompt = msg.hwnd == hDlg || IsChild(hDlg, msg.hwnd);
        if (belongsToPrompt && msg.message == WM_KEYDOWN) {
            if (msg.wParam == VK_RETURN) {
                WCHAR buf[512] = { 0 };
                GetWindowTextW(hEdit, buf, 511);
                outVal = buf;
                okClicked = true;
                running = false;
                continue;
            }
            if (msg.wParam == VK_ESCAPE) {
                running = false;
                continue;
            }
        }
        if (msg.hwnd == hDlg && msg.message == WM_COMMAND) {
            const int id = LOWORD(msg.wParam);
            if (id == IDOK) {
                WCHAR buf[512] = { 0 };
                GetWindowTextW(hEdit, buf, 511);
                outVal = buf;
                okClicked = true;
                running = false;
                continue;
            }
            if (id == IDCANCEL) {
                running = false;
                continue;
            }
        }

        if (belongsToPrompt && IsDialogMessageW(hDlg, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        if (!IsWindow(hDlg)) running = false;
    }

    EnableWindow(hParent, TRUE);
    SetFocus(hParent);
    if (IsWindow(hDlg)) DestroyWindow(hDlg);
    DeleteObject(hFont);
    DeleteObject(hFontBold);

    while (!outVal.empty() && (outVal.back() == L' ' || outVal.back() == L'\t')) outVal.pop_back();
    while (!outVal.empty() && (outVal.front() == L' ' || outVal.front() == L'\t')) outVal.erase(outVal.begin());

    return okClicked && !outVal.empty();
}

void SaveLogsToFile(HWND hwnd) {
    WCHAR szFile[MAX_PATH] = L"aether-minecraft.log";
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(WCHAR);
    ofn.lpstrFilter = L"Log Files (*.log)\0*.log\0Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = L"log";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameW(&ofn)) {
        FILE* fp = _wfopen(szFile, L"w, ccs=UTF-8");
        if (fp) {
            {
                std::lock_guard<std::mutex> lock(g_logsMutex);
                for (const auto& line : g_logs) {
                    fputws(line.c_str(), fp);
                    fputws(L"\n", fp);
                }
            }
            fclose(fp);
            AppendLog(Tr(L"[Aether] Лог успешно сохранен в файл: " + std::wstring(szFile),
                         L"[Aether] Logs successfully saved to file: " + std::wstring(szFile)));
        }
    }
}

void CopyLogsToClipboard(HWND hwnd) {
    std::wstring allText;
    {
        std::lock_guard<std::mutex> lock(g_logsMutex);
        for (const auto& line : g_logs) {
            allText += line + L"\r\n";
        }
    }
    if (OpenClipboard(hwnd)) {
        EmptyClipboard();
        size_t bytes = (allText.size() + 1) * sizeof(wchar_t);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (hMem) {
            void* pMem = GlobalLock(hMem);
            memcpy(pMem, allText.c_str(), bytes);
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        }
        CloseClipboard();
        AppendLog(Tr(L"[Aether] Все логи скопированы в буфер обмена!",
                     L"[Aether] All logs copied to clipboard!"));
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

    // Native C++ Win32 engine indicator removed as requested by user

    // ------------------------------------------------------------------------
    // 2. Top Header Bar (Height: 54px, Seamless Flow Without Harsh Border!)
    // ------------------------------------------------------------------------
    int topH = 54;
    int contentX = sidebarW + 24;
    int contentW = width - sidebarW - 48;

    SolidBrush topBrush(g_currentTheme.bgSurface);
    g.FillRectangle(&topBrush, sidebarW, 0, width - sidebarW, topH);

    // Clickable Active Container Selector Dropdown Pill
    DrawCard(g, (float)sidebarW + 20, 11, 280, 32, 8, g_currentTheme.bgSurfaceElevated, g_currentTheme.borderFocus);
    DrawIconGamepad(g, (float)sidebarW + 32, 18, 18, 16, g_currentTheme.accentSecondary);
    PointF activeNamePt((float)sidebarW + 60, 17);
    g.DrawString(g_activeContainerName.c_str(), -1, &fontBold, activeNamePt, &whiteBrush);
    PointF arrowPillPt((float)sidebarW + 276, 18);
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
    int contentY = topH + 16;

    // The central pane is a clipped vertical viewport. This preserves readable
    // card sizes on compact windows instead of shrinking every line of text.
    ClampTabScroll(height);
    GraphicsState tabContentState = g.Save();
    Rect tabViewport(sidebarW + 1, topH, std::max(1, width - sidebarW - 1), std::max(1, height - dockH - topH));
    g.SetClip(tabViewport, CombineModeReplace);
    g.TranslateTransform(0.0f, (float)-g_tabScrollOffset);

    if (g_currentTab == 0) {
        // --- Containers Tab (Only 1 Default Container with Selection) ---
        PointF titlePt((float)contentX, (float)contentY);
        g.DrawString(Tr(L"Игровые контейнеры", L"Game Containers"), -1, &fontTitle, titlePt, &whiteBrush);

        PointF subPt((float)contentX, (float)contentY + 28);
        g.DrawString(Tr(L"Изолированные профили для версий Minecraft, сборок и модов",
                        L"Isolated profiles for Minecraft versions, modpacks and loaders"),
                     -1, &fontSubtitle, subPt, &mutedBrush);

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

            DrawIconCube(g, (float)contentX + 36, (float)cY + 36, 18.0f,
                         g_currentTheme.accentSecondary, g_currentTheme.accent, Color(255, 100, 50, 160));

            PointF cTitlePt((float)contentX + 70, (float)cY + 16);
            g.DrawString(g_containers[i].name.c_str(), -1, &fontHeader, cTitlePt, &whiteBrush);

            DrawCard(g, (float)contentX + 70, (float)cY + 42, 65, 20, 4, Color(40, 16, 185, 129), Color(100, 16, 185, 129));
            RectF b1Rc((float)contentX + 70, (float)cY + 42, 65, 20);
            g.DrawString(g_containers[i].loader.c_str(), -1, &fontSmallBold, b1Rc, &sfCenter, &greenBrush);

            DrawCard(g, (float)contentX + 142, (float)cY + 42, 55, 20, 4, g_currentTheme.bgSurfaceElevated, g_currentTheme.border);
            RectF b2Rc((float)contentX + 142, (float)cY + 42, 55, 20);
            g.DrawString(g_containers[i].version.c_str(), -1, &fontSmall, b2Rc, &sfCenter, &mutedBrush);

            PointF cStatsPt((float)contentX + 70, (float)cY + 72);
            std::wstring stats = Tr(L"Выделенная память: ", L"Memory: ") + std::to_wstring(g_config.memoryMb / 1024) +
                                 L" GB  •  " + Tr(L"Загрузчик: ", L"Loader: ") + g_containers[i].loader +
                                 L"  •  " + Tr(L"Статус: Готов", L"Status: Ready");
            g.DrawString(stats.c_str(), -1, &fontSmall, cStatsPt, &mutedBrush);

            // Container rename button pill
            int renBtnX = contentX + 70;
            int renBtnY = cY + 92;
            int renBtnW = 125;
            DrawCard(g, (float)renBtnX, (float)renBtnY, (float)renBtnW, 20, 4, g_currentTheme.bgSurfaceElevated, g_currentTheme.border);
            RectF renRc((float)renBtnX, (float)renBtnY, (float)renBtnW, 20);
            g.DrawString(Tr(L"✏ Изменить имя", L"✏ Rename"), -1, &fontSmall, renRc, &sfCenter, &mutedBrush);

            std::vector<std::wstring> instJars = GetInstalledModFiles(g_config.storagePath, g_containers[i].id);
            std::wstring modCountText = Tr(L"Моды (", L"Mods (") + std::to_wstring(instJars.size()) + L")";

            int btnW = 160;
            int btnX = contentX + cardW - btnW - 16;

            // 1. Active / Select Button (Top)
            int actBtnY = cY + 14;
            if (isActive) {
                DrawCard(g, (float)btnX, (float)actBtnY, (float)btnW, 28, 6, g_currentTheme.accent, g_currentTheme.accent);
                DrawIconCheck(g, (float)btnX + 16, (float)actBtnY + 8, 12.0f, Color(255, 255, 255, 255));
                PointF selTextPt((float)btnX + 36, (float)actBtnY + 6);
                g.DrawString(Tr(L"Активен ✔", L"Active ✔"), -1, &fontSmallBold, selTextPt, &whiteBrush);
            } else {
                DrawCard(g, (float)btnX, (float)actBtnY, (float)btnW, 28, 6, g_currentTheme.bgSurfaceElevated, g_currentTheme.border);
                RectF selRc((float)btnX, (float)actBtnY, (float)btnW, 28);
                g.DrawString(Tr(L"Выбрать профиль", L"Select Profile"), -1, &fontSmallBold, selRc, &sfCenter, &mutedBrush);
            }

            // 2. Separate Installed Mods Button (Middle)
            int modsBtnY = cY + 48;
            DrawCard(g, (float)btnX, (float)modsBtnY, (float)btnW, 28, 6, g_currentTheme.bgSurfaceElevated, g_currentTheme.borderFocus);
            RectF modsRc((float)btnX, (float)modsBtnY, (float)btnW, 28);
            SolidBrush cyanPillBrush(Color(255, 56, 189, 248));
            g.DrawString(modCountText.c_str(), -1, &fontSmallBold, modsRc, &sfCenter, &cyanPillBrush);

            // 3. Quick Game Folder Button (Bottom - Screenshots, Saves, Mods)
            int folderBtnY = cY + 82;
            DrawCard(g, (float)btnX, (float)folderBtnY, (float)btnW, 28, 6, g_currentTheme.bgSurfaceElevated, g_currentTheme.border);
            RectF folderRc((float)btnX, (float)folderBtnY, (float)btnW, 28);
            g.DrawString(Tr(L"Папка с игрой", L"Game Folder"), -1, &fontSmallBold, folderRc, &sfCenter, &whiteBrush);
        }
    }
    else if (g_currentTab == 1) {
        // --- Modrinth Mods Tab (Search + infinite scrolling catalog + details view) ---
        if (g_selectedModDetail != nullptr) {
            // Mod Detail View
            DrawCard(g, (float)contentX, (float)contentY, 150, 30, 6, g_currentTheme.bgSurfaceElevated, g_currentTheme.borderFocus);
            PointF backPt((float)contentX + 14, (float)contentY + 6);
            g.DrawString(Tr(L"← Назад к каталогу", L"← Back to catalog"), -1, &fontSmallBold, backPt, &whiteBrush);

            int cardY = contentY + 44;
            int detailH = mainH - cardY - 10;
            DrawCard(g, (float)contentX, (float)cardY, (float)contentW, (float)detailH, 12, g_currentTheme.bgCard, g_currentTheme.border);

            PointF mTitlePt((float)contentX + 24, (float)cardY + 20);
            Font fontModLarge(&ff, 20, FontStyleBold, UnitPixel);
            g.DrawString(g_selectedModDetail->name.c_str(), -1, &fontModLarge, mTitlePt, &whiteBrush);

            DrawCard(g, (float)contentX + 24, (float)cardY + 54, 90, 22, 4, Color(40, 6, 182, 212), Color(100, 6, 182, 212));
            RectF catRc((float)contentX + 24, (float)cardY + 54, 90, 22);
            SolidBrush cyanBrush(Color(255, 56, 189, 248));
            g.DrawString(g_selectedModDetail->category.c_str(), -1, &fontSmallBold, catRc, &sfCenter, &cyanBrush);

            PointF dlPt((float)contentX + 124, (float)cardY + 56);
            std::wstring dlText = Tr(L"Скачиваний: ", L"Downloads: ") + g_selectedModDetail->downloads;
            g.DrawString(dlText.c_str(), -1, &fontSmall, dlPt, &mutedBrush);

            DrawCard(g, (float)contentX + 24, (float)cardY + 88, (float)contentW - 48, 110, 8, g_currentTheme.bgSurfaceElevated, g_currentTheme.border);
            RectF descRc((float)contentX + 36, (float)cardY + 98, (float)contentW - 72, 90);
            StringFormat sfDesc;
            sfDesc.SetTrimming(StringTrimmingWord);
            g.DrawString(g_selectedModDetail->fullDesc.c_str(), -1, &fontRegular, descRc, &sfDesc, &whiteBrush);

            int metaY = cardY + 214;
            PointF vLblPt((float)contentX + 24, (float)metaY);
            g.DrawString(Tr(L"Поддерживаемые версии Minecraft:", L"Supported Minecraft versions:"), -1, &fontBold, vLblPt, &whiteBrush);
            PointF vValPt((float)contentX + 24, (float)metaY + 20);
            g.DrawString(g_selectedModDetail->versions.c_str(), -1, &fontRegular, vValPt, &mutedBrush);

            PointF ldrLblPt((float)contentX + 24, (float)metaY + 48);
            g.DrawString(Tr(L"Загрузчик модов:", L"Mod Loader:"), -1, &fontBold, ldrLblPt, &whiteBrush);
            PointF ldrValPt((float)contentX + 24, (float)metaY + 68);
            g.DrawString(g_selectedModDetail->loader.c_str(), -1, &fontRegular, ldrValPt, &mutedBrush);

            PointF authLblPt((float)contentX + 240, (float)metaY + 48);
            g.DrawString(Tr(L"Автор мода:", L"Author:"), -1, &fontBold, authLblPt, &whiteBrush);
            PointF authValPt((float)contentX + 240, (float)metaY + 68);
            g.DrawString(g_selectedModDetail->author.c_str(), -1, &fontRegular, authValPt, &mutedBrush);

            int actY = cardY + detailH - 52;
            int instBtnW = 160;
            if (g_selectedModDetail->installed) {
                DrawCard(g, (float)contentX + 24, (float)actY, (float)instBtnW, 36, 6, Color(40, 16, 185, 129), g_currentTheme.success);
                DrawIconCheck(g, (float)contentX + 40, (float)actY + 12, 12.0f, g_currentTheme.success);
                PointF instTxtPt((float)contentX + 58, (float)actY + 8);
                SolidBrush greenText(g_currentTheme.success);
                g.DrawString(Tr(L"Установлен", L"Installed"), -1, &fontBold, instTxtPt, &greenText);
            } else {
                DrawGradientButton(g, (float)contentX + 24, (float)actY, (float)instBtnW, 36, 6,
                                   g_currentTheme.accent, g_currentTheme.accentSecondary,
                                   Color(255, 255, 255, 255), Tr(L"+ Установить мод", L"+ Install Mod"), fontBold, false, false);
            }

            DrawCard(g, (float)contentX + 196, (float)actY, 210, 36, 6, g_currentTheme.bgSurfaceElevated, g_currentTheme.borderFocus);
            PointF linkPt((float)contentX + 210, (float)actY + 8);
            g.DrawString(Tr(L"Страница на Modrinth ↗", L"Modrinth Page ↗"), -1, &fontSmallBold, linkPt, &whiteBrush);
        }
        else {
            PointF titlePt((float)contentX, (float)contentY);
            g.DrawString(g_showInstalledMods ? Tr(L"Установленные моды", L"Installed Mods")
                                             : Tr(L"Каталог Modrinth", L"Modrinth Catalog"),
                         -1, &fontTitle, titlePt, &whiteBrush);

            PointF subPt((float)contentX, (float)contentY + 28);
            g.DrawString(g_showInstalledMods
                            ? Tr(L"Файлы .jar, реально найденные в папке mods активного контейнера.",
                                 L"JAR files actually found in the active container's mods folder.")
                            : Tr(L"Поиск модов по названию. Совместимость подбирается для выбранного контейнера.",
                                 L"Search mods by name. Compatibility is selected for the active container."),
                         -1, &fontSubtitle, subPt, &mutedBrush);

            // Search stays a name-only field. The adjacent explicit toggle
            // prevents the old hidden category state from making the online
            // catalog appear blank after opening installed mods from a card.
            const int searchY = contentY + 58;
            const int installedBtnW = std::min(160, std::max(132, contentW / 4));
            const int searchW = std::max(120, contentW - installedBtnW - 8);
            DrawCard(g, (float)contentX, (float)searchY, (float)searchW, 34, 8,
                     g_currentTheme.bgSurfaceElevated, g_currentTheme.borderFocus);
            DrawIconSearch(g, (float)contentX + 12, (float)searchY + 9, 16.0f, g_currentTheme.accentSecondary);

            const int installedBtnX = contentX + searchW + 8;
            DrawCard(g, (float)installedBtnX, (float)searchY, (float)installedBtnW, 34, 8,
                     g_showInstalledMods ? g_currentTheme.accent : g_currentTheme.bgSurfaceElevated,
                     g_showInstalledMods ? g_currentTheme.accent : g_currentTheme.border);
            RectF installedToggleRc((float)installedBtnX, (float)searchY, (float)installedBtnW, 34);
            SolidBrush installedToggleBrush(g_showInstalledMods ? Color(255, 255, 255, 255) : g_currentTheme.textMuted);
            g.DrawString(g_showInstalledMods ? Tr(L"← Каталог", L"← Catalog")
                                             : Tr(L"Установленные моды", L"Installed Mods"),
                         -1, &fontSmallBold, installedToggleRc, &sfCenter, &installedToggleBrush);

            const int modListY = searchY + 52;
            const int modItemH = 72;
            if (g_showInstalledMods) {
                std::vector<std::wstring> installedJars;
                if (g_activeContainerIndex >= 0 && g_activeContainerIndex < (int)g_containers.size()) {
                    installedJars = GetInstalledModFiles(g_config.storagePath, g_containers[g_activeContainerIndex].id);
                }
                const std::wstring query = ToLowerW(g_modSearchQuery);
                int visibleIndex = 0;
                for (const std::wstring& jar : installedJars) {
                    if (!query.empty() && ToLowerW(jar).find(query) == std::wstring::npos) continue;
                    const int mY = modListY + visibleIndex * (modItemH + 8);
                    DrawCard(g, (float)contentX, (float)mY, (float)contentW, (float)modItemH, 8,
                             g_currentTheme.bgCard, g_currentTheme.border);
                    PointF mNamePt((float)contentX + 16, (float)mY + 12);
                    g.DrawString(jar.c_str(), -1, &fontBold, mNamePt, &whiteBrush);
                    PointF locationPt((float)contentX + 16, (float)mY + 38);
                    g.DrawString(Tr(L"Обнаружен на диске: instances / активный контейнер / mods",
                                    L"Detected on disk: instances / active container / mods"),
                                 -1, &fontSmall, locationPt, &mutedBrush);
                    const int diskBtnW = 104;
                    const int diskBtnX = contentX + contentW - diskBtnW - 12;
                    DrawCard(g, (float)diskBtnX, (float)mY + 22, (float)diskBtnW, 28, 6,
                             Color(40, 16, 185, 129), g_currentTheme.success);
                    RectF diskRc((float)diskBtnX, (float)mY + 22, (float)diskBtnW, 28);
                    SolidBrush diskBrush(g_currentTheme.success);
                    g.DrawString(Tr(L"На диске", L"On disk"), -1, &fontSmallBold, diskRc, &sfCenter, &diskBrush);
                    ++visibleIndex;
                }
                if (visibleIndex == 0) {
                    PointF emptyPt((float)contentX + 16, (float)modListY + 18);
                    g.DrawString(query.empty()
                                    ? Tr(L"В этом контейнере пока нет установленных модов.",
                                         L"This container has no installed mods yet.")
                                    : Tr(L"Среди установленных файлов нет совпадений.",
                                         L"No installed files match this name."),
                                 -1, &fontRegular, emptyPt, &mutedBrush);
                }
            } else {
                if (g_filteredMods.empty() && !g_modCatalogLoading.load()) {
                    PointF emptyPt((float)contentX + 16, (float)modListY + 18);
                    g.DrawString(Tr(L"Моды не найдены. Измените название или проверьте версию и загрузчик контейнера.",
                                    L"No mods found. Change the name or check the container version and loader."),
                                 -1, &fontRegular, emptyPt, &mutedBrush);
                }

                for (int i = 0; i < (int)g_filteredMods.size(); ++i) {
                    const int mY = modListY + i * (modItemH + 8);
                    const ModItem& mod = g_filteredMods[i];
                    DrawCard(g, (float)contentX, (float)mY, (float)contentW, (float)modItemH, 8,
                             g_currentTheme.bgCard, g_currentTheme.border);

                    PointF mNamePt((float)contentX + 16, (float)mY + 10);
                    g.DrawString(mod.name.c_str(), -1, &fontBold, mNamePt, &whiteBrush);

                    const int detBtnW = 86;
                    const int btnW = 120;
                    const int btnX = contentX + contentW - btnW - 12;
                    const int detBtnX = btnX - detBtnW - 8;
                    const int descriptionW = std::max(80, detBtnX - contentX - 28);
                    RectF mDescRc((float)contentX + 16, (float)mY + 32, (float)descriptionW, 30);
                    StringFormat briefFormat;
                    briefFormat.SetTrimming(StringTrimmingEllipsisCharacter);
                    briefFormat.SetFormatFlags(StringFormatFlagsLineLimit);
                    g.DrawString(mod.brief.c_str(), -1, &fontSmall, mDescRc, &briefFormat, &mutedBrush);

                    DrawCard(g, (float)detBtnX, (float)mY + 22, (float)detBtnW, 28, 6,
                             g_currentTheme.bgSurfaceElevated, g_currentTheme.border);
                    RectF detRc((float)detBtnX, (float)mY + 22, (float)detBtnW, 28);
                    g.DrawString(Tr(L"Инфо", L"Details"), -1, &fontSmallBold, detRc, &sfCenter, &whiteBrush);

                    if (mod.isDownloading) {
                        DrawCard(g, (float)btnX, (float)mY + 22, (float)btnW, 28, 6,
                                 g_currentTheme.bgSurfaceElevated, g_currentTheme.accent);
                        RectF dlRc((float)btnX, (float)mY + 22, (float)btnW, 28);
                        SolidBrush dlBrush(g_currentTheme.accentSecondary);
                        g.DrawString(Tr(L"Загрузка...", L"Downloading..."), -1, &fontSmallBold, dlRc, &sfCenter, &dlBrush);
                    } else if (mod.installed) {
                        DrawCard(g, (float)btnX, (float)mY + 22, (float)btnW, 28, 6,
                                 Color(40, 16, 185, 129), g_currentTheme.success);
                        RectF installedRc((float)btnX, (float)mY + 22, (float)btnW, 28);
                        SolidBrush installedBrush(g_currentTheme.success);
                        g.DrawString(Tr(L"Установлен", L"Installed"), -1, &fontSmallBold, installedRc, &sfCenter, &installedBrush);
                    } else {
                        DrawGradientButton(g, (float)btnX, (float)mY + 22, (float)btnW, 28, 6,
                                           g_currentTheme.accent, g_currentTheme.accentSecondary,
                                           Color(255, 255, 255, 255), Tr(L"Скачать", L"Download"), fontSmallBold, false, false);
                    }
                }

                const int statusY = modListY + (int)g_filteredMods.size() * (modItemH + 8);
                DrawCard(g, (float)contentX, (float)statusY, (float)contentW, 34, 8,
                         g_currentTheme.bgSurfaceElevated, g_currentTheme.border);
                RectF statusRc((float)contentX + 12, (float)statusY, (float)contentW - 24, 34);
                std::wstring status;
                if (g_modCatalogLoading.load()) {
                    status = Tr(L"Загружаем ещё 20 модов…", L"Loading 20 more mods…");
                } else if (g_modCatalogHasMore) {
                    status = Tr(L"Прокрутите вниз — каталог автоматически подгрузит следующую партию.",
                                L"Scroll down — the catalog will load the next batch automatically.");
                } else {
                    status = Tr(L"Показаны все найденные моды.", L"All matching mods are shown.");
                }
                g.DrawString(status.c_str(), -1, &fontSmallBold, statusRc, &sfCenter, &mutedBrush);
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

        // Save & Copy Buttons
        int saveBtnW = 140;
        int copyBtnW = 110;
        int btnGap = 8;
        int copyBtnX = contentX + contentW - copyBtnW;
        int saveBtnX = copyBtnX - saveBtnW - btnGap;
        int logBtnY = contentY + 12;

        DrawCard(g, (float)saveBtnX, (float)logBtnY, (float)saveBtnW, 28, 6, g_currentTheme.bgSurfaceElevated, g_currentTheme.borderFocus);
        RectF saveRc((float)saveBtnX, (float)logBtnY, (float)saveBtnW, 28);
        g.DrawString(Tr(L"Скачать лог", L"Download Log"), -1, &fontSmallBold, saveRc, &sfCenter, &whiteBrush);

        DrawCard(g, (float)copyBtnX, (float)logBtnY, (float)copyBtnW, 28, 6, g_currentTheme.bgSurfaceElevated, g_currentTheme.border);
        RectF copyRc((float)copyBtnX, (float)logBtnY, (float)copyBtnW, 28);
        g.DrawString(Tr(L"Копировать", L"Copy All"), -1, &fontSmallBold, copyRc, &sfCenter, &mutedBrush);

        int termH = mainH - logY - 14;
        DrawCard(g, (float)contentX, (float)logY, (float)contentW, (float)termH, 8, Color(255, 10, 11, 16), g_currentTheme.border);

        FontFamily ffMono(L"Consolas");
        Font fontMono(&ffMono, 11, FontStyleRegular, UnitPixel);

        std::lock_guard<std::mutex> lock(g_logsMutex);
        int totalLogs = (int)g_logs.size();
        int visibleLines = (termH - 24) / 18;
        if (visibleLines < 1) visibleLines = 1;
        int maxOffset = std::max(0, totalLogs - visibleLines);

        int curOffset = g_logScrollOffset;
        if (curOffset < 0 || curOffset > maxOffset) {
            curOffset = maxOffset;
        }

        int endIdx = std::min(totalLogs, curOffset + visibleLines);
        for (int i = curOffset; i < endIdx; ++i) {
            int lineY = logY + 12 + (i - curOffset) * 18;
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

        // Draw Interactive Scrollbar
        if (totalLogs > visibleLines) {
            int sbX = contentX + contentW - 12;
            int sbY = logY + 4;
            int sbH = termH - 8;
            int thumbH = std::max(20, (visibleLines * sbH) / totalLogs);
            int thumbY = sbY + (curOffset * (sbH - thumbH)) / maxOffset;

            SolidBrush sbTrackBrush(Color(40, 255, 255, 255));
            g.FillRectangle(&sbTrackBrush, sbX + 2, sbY, 4, sbH);

            SolidBrush sbThumbBrush(g_currentTheme.accent);
            GraphicsPath thumbPath;
            AddRoundRect(thumbPath, (float)sbX, (float)thumbY, 8.0f, (float)thumbH, 4);
            g.FillPath(&sbThumbBrush, &thumbPath);

            // Floating "Down" pill button if scrolled up
            if (curOffset < maxOffset) {
                int downBtnX = contentX + contentW - 100;
                int downBtnY = logY + termH - 34;
                DrawCard(g, (float)downBtnX, (float)downBtnY, 85, 24, 6, g_currentTheme.bgSurfaceElevated, g_currentTheme.borderFocus);
                RectF dRc((float)downBtnX, (float)downBtnY, 85, 24);
                g.DrawString(Tr(L"В конец", L"To bottom"), -1, &fontSmallBold, dRc, &sfCenter, &whiteBrush);
            }
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
        // --- Settings Tab: RAM Draggable Slider & Full Java / JVM Customization ---
        PointF titlePt((float)contentX, (float)contentY);
        g.DrawString(Tr(L"Настройки лаунчера и параметры Java", L"Launcher & Java Settings"), -1, &fontTitle, titlePt, &whiteBrush);

        PointF subPt((float)contentX, (float)contentY + 28);
        g.DrawString(Tr(L"Выделение памяти (RAM), произвольные JVM флаги, сборщик мусора и Java рантайм",
                        L"Memory allocation (RAM), custom JVM arguments, garbage collector & Java runtime"),
                     -1, &fontSubtitle, subPt, &mutedBrush);

        // 1. RAM Allocation Card (Global across all containers)
        int setY = contentY + 62;
        DrawCard(g, (float)contentX, (float)setY, (float)contentW, 110, 10, g_currentTheme.bgCard, g_currentTheme.border);

        PointF ramLblPt((float)contentX + 16, (float)setY + 12);
        g.DrawString(Tr(L"Выделенная память (RAM) — для всех контейнеров", L"Allocated RAM — across all containers"), -1, &fontBold, ramLblPt, &whiteBrush);

        PointF ramCustomLblPt((float)contentX + 16, (float)setY + 40);
        g.DrawString(Tr(L"Кастомный ввод:", L"Custom Input:"), -1, &fontSmall, ramCustomLblPt, &mutedBrush);

        DrawCard(g, (float)contentX + 120, (float)setY + 34, 90, 26, 6, g_currentTheme.bgSurfaceElevated, g_currentTheme.borderFocus);
        PointF mbLblPt((float)contentX + 218, (float)setY + 40);
        g.DrawString(L"MB", -1, &fontSmallBold, mbLblPt, &mutedBrush);

        float gbVal = (float)g_config.memoryMb / 1024.0f;
        std::wstringstream gbs;
        gbs << std::fixed << std::setprecision(1) << gbVal << L" GB";
        PointF gbPt((float)contentX + 250, (float)setY + 40);
        SolidBrush accentSec(g_currentTheme.accentSecondary);
        g.DrawString((L"(" + gbs.str() + L")").c_str(), -1, &fontBold, gbPt, &accentSec);

        int sliderX = contentX + 16;
        int sliderY = setY + 68;
        int sliderW = contentW - 32;
        int sliderH = 6;

        GraphicsPath trackPath;
        AddRoundRect(trackPath, (float)sliderX, (float)sliderY, (float)sliderW, (float)sliderH, 3);
        SolidBrush trackBg(g_currentTheme.bgSurfaceElevated);
        g.FillPath(&trackBg, &trackPath);

        float ramPct = (float)(g_config.memoryMb - 1024) / (float)(16384 - 1024);
        if (ramPct < 0.0f) ramPct = 0.0f;
        if (ramPct > 1.0f) ramPct = 1.0f;
        float fillW = sliderW * ramPct;

        if (fillW > 4.0f) {
            GraphicsPath fillPath;
            AddRoundRect(fillPath, (float)sliderX, (float)sliderY, fillW, (float)sliderH, 3);
            LinearGradientBrush fillGrad(PointF((float)sliderX, (float)sliderY),
                                        PointF((float)sliderX + fillW, (float)sliderY),
                                        g_currentTheme.accent, g_currentTheme.accentSecondary);
            g.FillPath(&fillGrad, &fillPath);
        }

        float knobX = sliderX + fillW;
        float knobY = sliderY + 3.0f;
        SolidBrush knobBrush(g_currentTheme.accentSecondary);
        g.FillEllipse(&knobBrush, knobX - 7.0f, knobY - 7.0f, 14.0f, 14.0f);
        Pen knobPen(Color(255, 255, 255, 255), 2.0f);
        g.DrawEllipse(&knobPen, knobX - 7.0f, knobY - 7.0f, 14.0f, 14.0f);

        int pillStartX = contentX + 16;
        int pillY = setY + 82;
        for (size_t i = 0; i < g_ramPresets.size(); ++i) {
            int pX = pillStartX + (int)i * 72;
            bool isSel = (g_config.memoryMb == g_ramPresets[i]);

            if (isSel) {
                DrawCard(g, (float)pX, (float)pillY, 62, 22, 4, g_currentTheme.accent, g_currentTheme.accent);
            } else {
                DrawCard(g, (float)pX, (float)pillY, 62, 22, 4, g_currentTheme.bgSurfaceElevated, g_currentTheme.border);
            }

            std::wstring ramLabel = std::to_wstring(g_ramPresets[i] / 1024) + L" GB";
            RectF ramRc((float)pX, (float)pillY, 62, 22);
            SolidBrush ramTextBrush(isSel ? Color(255, 255, 255, 255) : g_currentTheme.textMuted);
            g.DrawString(ramLabel.c_str(), -1, &fontSmallBold, ramRc, &sfCenter, &ramTextBrush);
        }

        // 2. Full Java Runtime & Custom JVM Flags Tuning Card
        int jvmCardY = setY + 118;
        const int jvmCardH = GetSettingsJvmCardHeight(contentW);
        DrawCard(g, (float)contentX, (float)jvmCardY, (float)contentW, (float)jvmCardH, 10, g_currentTheme.bgCard, g_currentTheme.border);

        PointF jvmTitlePt((float)contentX + 16, (float)jvmCardY + 12);
        g.DrawString(Tr(L"Параметры Java и флаги JVM (Настройка на уровне движка Java)",
                        L"Java Runtime & JVM Flags (Java-level engine customization)"), -1, &fontBold, jvmTitlePt, &whiteBrush);

        PointF jvmSubPt((float)contentX + 16, (float)jvmCardY + 34);
        g.DrawString(Tr(L"Пресеты сборщика мусора (GC):", L"Garbage Collector (GC) Presets:"), -1, &fontSmall, jvmSubPt, &mutedBrush);

        // GC Preset Pills
        struct GcPresetItem { std::wstring id; std::wstring label; };
        const std::vector<std::wstring> gcLabels = GetSettingsGcPresetLabels();
        std::vector<GcPresetItem> gcPresets = {
            { L"aikar", gcLabels[0] },
            { L"zgc", gcLabels[1] },
            { L"shenandoah", gcLabels[2] },
            { L"standard", gcLabels[3] },
            { L"custom", gcLabels[4] }
        };

        int gcPillX = contentX + 16;
        int gcPillY = jvmCardY + 54;
        const int gcPillRight = contentX + contentW - 16;
        for (const auto& pr : gcPresets) {
            bool isPrSel = (g_config.jvmPreset == pr.id);
            int pillW = GetSettingsGcPillWidth(pr.label);
            if (gcPillX + pillW > gcPillRight && gcPillX != contentX + 16) {
                gcPillX = contentX + 16;
                gcPillY += 30;
            }

            DrawCard(g, (float)gcPillX, (float)gcPillY, (float)pillW, 24, 4,
                     isPrSel ? g_currentTheme.accent : g_currentTheme.bgSurfaceElevated,
                     isPrSel ? g_currentTheme.accent : g_currentTheme.border);

            RectF prRc((float)gcPillX, (float)gcPillY, (float)pillW, 24);
            SolidBrush prBrush(isPrSel ? Color(255, 255, 255, 255) : g_currentTheme.textMuted);
            g.DrawString(pr.label.c_str(), -1, &fontSmallBold, prRc, &sfCenter, &prBrush);

            gcPillX += pillW + 6;
        }

        // Custom JVM Arguments input box (g_hEditJvmArgs)
        PointF jvmArgsLblPt((float)contentX + 16, (float)jvmCardY + GetSettingsJvmArgsLabelOffset(contentW));
        g.DrawString(Tr(L"Пользовательские аргументы / флаги JVM (полная свобода настройки):",
                        L"Custom JVM Arguments / Flags (complete customization freedom):"), -1, &fontSmall, jvmArgsLblPt, &mutedBrush);

        DrawCard(g, (float)contentX + 12, (float)jvmCardY + GetSettingsJvmArgsInputOffset(contentW), (float)contentW - 24, 28, 6, g_currentTheme.bgSurfaceElevated, g_currentTheme.borderFocus);

        // Custom Java Path input box (g_hEditJavaPath) & Auto / Test buttons
        PointF javaPathLblPt((float)contentX + 16, (float)jvmCardY + GetSettingsJavaPathLabelOffset(contentW));
        g.DrawString(Tr(L"Путь к Java (javaw.exe):", L"Java Runtime Executable Path:"), -1, &fontSmall, javaPathLblPt, &mutedBrush);

        DrawCard(g, (float)contentX + 12, (float)jvmCardY + GetSettingsJavaPathInputOffset(contentW), (float)contentW - 212, 28, 6, g_currentTheme.bgSurfaceElevated, g_currentTheme.borderFocus);

        // Auto-detect Button
        int autoBtnX = contentX + contentW - 192;
        DrawCard(g, (float)autoBtnX, (float)jvmCardY + GetSettingsJavaPathInputOffset(contentW), 88, 28, 6, g_currentTheme.bgSurfaceElevated, g_currentTheme.border);
        RectF autoRc((float)autoBtnX, (float)jvmCardY + GetSettingsJavaPathInputOffset(contentW), 88, 28);
        g.DrawString(Tr(L"Автоподбор", L"Auto-detect"), -1, &fontSmallBold, autoRc, &sfCenter, &whiteBrush);

        // Test Java Button
        int testBtnX = contentX + contentW - 98;
        DrawGradientButton(g, (float)testBtnX, (float)jvmCardY + GetSettingsJavaPathInputOffset(contentW), 88, 28, 6,
                           g_currentTheme.accent, g_currentTheme.accentSecondary,
                           Color(255, 255, 255, 255), Tr(L"Тест Java", L"Test Java"), fontSmallBold, false, false);

        // 3. Display Resolution & Optimization Card
        int optCardY = jvmCardY + jvmCardH + 8;
        const int optCardH = GetSettingsOptionsCardHeight(contentW);
        const bool compactOptions = contentW < 680;
        DrawCard(g, (float)contentX, (float)optCardY, (float)contentW, (float)optCardH, 10, g_currentTheme.bgCard, g_currentTheme.border);

        PointF resLblPt((float)contentX + 16, (float)optCardY + 12);
        g.DrawString(Tr(L"Разрешение окна игры:", L"Game Window Resolution:"), -1, &fontSmallBold, resLblPt, &whiteBrush);

        DrawCard(g, (float)contentX + 136, (float)optCardY + 28, 70, 26, 4, g_currentTheme.bgSurfaceElevated, g_currentTheme.borderFocus);
        PointF xPt((float)contentX + 212, (float)optCardY + 32);
        g.DrawString(L"×", -1, &fontBold, xPt, &mutedBrush);
        DrawCard(g, (float)contentX + 220, (float)optCardY + 28, 70, 26, 4, g_currentTheme.bgSurfaceElevated, g_currentTheme.borderFocus);

        const int fsBtnX = compactOptions ? contentX + 16 : contentX + 310;
        const int fsBtnY = compactOptions ? optCardY + 62 : optCardY + 26;
        const bool stackCompactToggles = contentW < 390;
        const int cpuBtnX = compactOptions ? (stackCompactToggles ? fsBtnX : fsBtnX + 168) : contentX + 480;
        const int cpuBtnY = stackCompactToggles ? fsBtnY + 34 : fsBtnY;
        DrawCard(g, (float)fsBtnX, (float)fsBtnY, 160, 28, 6,
                 g_config.fullscreen ? g_currentTheme.accent : g_currentTheme.bgSurfaceElevated,
                 g_config.fullscreen ? g_currentTheme.accent : g_currentTheme.border);
        RectF fsRc((float)fsBtnX, (float)fsBtnY, 160, 28);
        std::wstring fsTxt = g_config.fullscreen ? Tr(L"✓ На весь экран: ВКЛ", L"✓ Fullscreen: ON") : Tr(L"На весь экран: ВЫКЛ", L"Fullscreen: OFF");
        SolidBrush fsBrush(g_config.fullscreen ? Color(255, 255, 255, 255) : g_currentTheme.textMuted);
        g.DrawString(fsTxt.c_str(), -1, &fontSmallBold, fsRc, &sfCenter, &fsBrush);

        DrawCard(g, (float)cpuBtnX, (float)cpuBtnY, 175, 28, 6,
                 g_config.lowCpuMode ? Color(40, 16, 185, 129) : g_currentTheme.bgSurfaceElevated,
                 g_config.lowCpuMode ? g_currentTheme.success : g_currentTheme.border);
        RectF cpuRc((float)cpuBtnX, (float)cpuBtnY, 175, 28);
        std::wstring cpuTxt = g_config.lowCpuMode ? Tr(L"✓ Низкая нагрузка CPU", L"✓ Low CPU Mode: ON") : Tr(L"Низкая нагрузка CPU: ВЫКЛ", L"Low CPU Mode: OFF");
        SolidBrush cpuBrush(g_config.lowCpuMode ? g_currentTheme.success : g_currentTheme.textMuted);
        g.DrawString(cpuTxt.c_str(), -1, &fontSmallBold, cpuRc, &sfCenter, &cpuBrush);

        // 4. Themes Card: fixed, readable pills flow to a new line when needed.
        const int themeY = optCardY + optCardH + 8;
        const int themeCardH = GetSettingsThemeCardHeight(contentW);
        DrawCard(g, (float)contentX, (float)themeY, (float)contentW, (float)themeCardH, 8, g_currentTheme.bgCard, g_currentTheme.border);
        PointF thmLblPt((float)contentX + 16, (float)themeY + 10);
        g.DrawString(Tr(L"Тема оформления:", L"Visual Theme:"), -1, &fontSmallBold, thmLblPt, &whiteBrush);

        const int themePillW = 114;
        const int themeGap = 8;
        const int themeRight = contentX + contentW - 16;
        int tX = contentX + 16;
        int tY = themeY + 32;
        for (size_t i = 0; i < g_themes.size(); ++i) {
            if (tX + themePillW > themeRight && tX != contentX + 16) {
                tX = contentX + 16;
                tY += 34;
            }
            bool isSel = (g_currentTheme.id == g_themes[i].id);
            DrawCard(g, (float)tX, (float)tY, themePillW, 26, 4,
                     isSel ? g_themes[i].accent : g_currentTheme.bgSurfaceElevated,
                     isSel ? g_themes[i].accent : g_currentTheme.border);
            RectF thmRc((float)tX, (float)tY, themePillW, 26);
            SolidBrush thmTextBrush(isSel ? Color(255, 255, 255, 255) : g_currentTheme.textMuted);
            g.DrawString(g_themes[i].name.c_str(), -1, &fontSmallBold, thmRc, &sfCenter, &thmTextBrush);
            tX += themePillW + themeGap;
        }
    }

    // Content must not draw over the fixed bottom launch dock.
    g.Restore(tabContentState);

    // A slim, fixed scrollbar communicates that the whole tab is scrollable.
    if (g_currentTab != 2) {
        const int scrollMax = GetTabScrollMax(height);
        if (scrollMax > 0) {
            const int trackX = width - 16;
            const int trackY = topH + 8;
            const int trackH = std::max(20, height - dockH - topH - 16);
            const int viewportH = std::max(1, height - dockH - topH);
            const int contentH = viewportH + scrollMax;
            const int thumbH = std::max(28, (viewportH * trackH) / contentH);
            const int thumbY = trackY + (g_tabScrollOffset * (trackH - thumbH)) / scrollMax;
            SolidBrush trackBrush(Color(35, 255, 255, 255));
            g.FillRectangle(&trackBrush, trackX + 3, trackY, 3, trackH);
            GraphicsPath thumbPath;
            AddRoundRect(thumbPath, (float)trackX, (float)thumbY, 8.0f, (float)thumbH, 4.0f);
            SolidBrush thumbBrush(g_currentTheme.accent);
            g.FillPath(&thumbBrush, &thumbPath);
        }
    }

    // ------------------------------------------------------------------------
    // 4. Bottom Launch Dock (Height: 84px) - Features Dedicated Loader Selector!
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
    std::wstring subVer = g_activeVersion + L" (" + g_config.loader + L") • " + Tr(L"Готов к игре", L"Ready to launch");
    g.DrawString(subVer.c_str(), -1, &fontSmall, dVerPt, &mutedBrush);

    // Center-Left: Prominent NICKNAME Input Box on Bottom Dock!
    PointF nickLblPt(250, (float)dockY + 14);
    SolidBrush accentSecBrush(g_currentTheme.accentSecondary);
    g.DrawString(Tr(L"НИКНЕЙМ ИГРОКА", L"PLAYER NICKNAME"), -1, &fontSmallBold, nickLblPt, &accentSecBrush);

    DrawCard(g, 248, (float)dockY + 34, 140, 32, 6, g_currentTheme.bgSurfaceElevated, g_currentTheme.borderFocus);

    // Center: Version Dropdown Selector Pill
    PointF verLblPt(402, (float)dockY + 14);
    g.DrawString(Tr(L"ВЕРСИЯ ИГРЫ", L"GAME VERSION"), -1, &fontSmallBold, verLblPt, &mutedBrush);

    DrawCard(g, 400, (float)dockY + 34, 105, 32, 6, g_currentTheme.bgSurfaceElevated, g_currentTheme.borderFocus);
    PointF verValPt(412, (float)dockY + 41);
    g.DrawString(g_activeVersion.c_str(), -1, &fontBold, verValPt, &whiteBrush);
    PointF arrowPt(488, (float)dockY + 42);
    g.DrawString(L"▼", -1, &fontSmall, arrowPt, &mutedBrush);

    // Center-Right: Mod Loader Selector Dropdown Pill!
    PointF ldrLblPt(520, (float)dockY + 14);
    g.DrawString(Tr(L"ЗАГРУЗЧИК", L"MOD LOADER"), -1, &fontSmallBold, ldrLblPt, &mutedBrush);

    DrawCard(g, 518, (float)dockY + 34, 120, 32, 6, g_currentTheme.bgSurfaceElevated, g_currentTheme.borderFocus);
    std::wstring ldrDisplayName = L"Vanilla";
    if (g_config.loader == L"fabric") ldrDisplayName = L"Fabric";
    else if (g_config.loader == L"forge") ldrDisplayName = L"Forge";
    else if (g_config.loader == L"neoforge") ldrDisplayName = L"NeoForge";
    else if (g_config.loader == L"quilt") ldrDisplayName = L"Quilt";

    PointF ldrValPt(530, (float)dockY + 41);
    g.DrawString(ldrDisplayName.c_str(), -1, &fontBold, ldrValPt, &whiteBrush);
    PointF ldrArrowPt(618, (float)dockY + 42);
    g.DrawString(L"▼", -1, &fontSmall, ldrArrowPt, &mutedBrush);

    // Status Pill
    int statusX = 652;
    DrawCard(g, (float)statusX, (float)dockY + 34, 175, 32, 16, g_currentTheme.bgCard, g_currentTheme.border);

    SolidBrush statusDotBrush(g_isGameRunning.load() ? g_currentTheme.warning : g_currentTheme.success);
    g.FillEllipse(&statusDotBrush, statusX + 14, dockY + 45, 10, 10);

    PointF stTextPt((float)statusX + 32, (float)dockY + 41);
    g.DrawString(g_launchStatus.c_str(), -1, &fontSmallBold, stTextPt, &whiteBrush);

    // ------------------------------------------------------------------------
    // THE PLAY BUTTON (Giant glowing rounded gradient button)
    // ------------------------------------------------------------------------
    int playBtnW = 190;
    int playBtnH = 52;
    int playBtnX = width - playBtnW - 20;
    int playBtnY = dockY + 16;

    GraphicsPath glowPath;
    AddRoundRect(glowPath, (float)playBtnX - 4, (float)playBtnY - 4, (float)playBtnW + 8, (float)playBtnH + 8, 16);

    if (g_isGameRunning.load()) {
        SolidBrush glowBrush(Color(70, 239, 68, 68));
        g.FillPath(&glowBrush, &glowPath);

        const wchar_t* stopText = Tr(L"⏹  ОСТАНОВИТЬ", L"⏹  STOP GAME");
        Color stopGrad1(255, 239, 68, 68);
        Color stopGrad2(255, 185, 28, 28);
        DrawGradientButton(g, (float)playBtnX, (float)playBtnY, (float)playBtnW, (float)playBtnH, 14,
                           stopGrad1, stopGrad2,
                           Color(255, 255, 255, 255), stopText, fontPlayBtn,
                           g_hoverPlayBtn, g_pressPlayBtn);
    } else {
        SolidBrush glowBrush(g_currentTheme.accentGlow);
        g.FillPath(&glowBrush, &glowPath);

        const wchar_t* playText = Tr(L"▶  ИГРАТЬ", L"▶  PLAY");
        DrawGradientButton(g, (float)playBtnX, (float)playBtnY, (float)playBtnW, (float)playBtnH, 14,
                           g_currentTheme.accent, g_currentTheme.accentSecondary,
                           Color(255, 255, 255, 255), playText, fontPlayBtn,
                           g_hoverPlayBtn, g_pressPlayBtn);
    }

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
        AppendLog(Tr(L"[Aether] Загрузчик модов: " + g_config.loader,
                     L"[Aether] Mod loader: " + g_config.loader));
        AppendLog(Tr(L"[Aether] Игрок: " + g_config.nickname,
                     L"[Aether] Player: " + g_config.nickname));
        AppendLog(Tr(L"[Aether] Выделение RAM (глобально): " + std::to_wstring(g_config.memoryMb) + L" MB",
                     L"[Aether] RAM (global): " + std::to_wstring(g_config.memoryMb) + L" MB"));

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
            g_config.loader,
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

        std::wstring containerId = g_containers[g_activeContainerIndex].id;
        std::wstring containerDir = GetContainerDir(g_config.storagePath, containerId);
        EnsureContainerStructure(g_config.storagePath, containerId);

        bool launched = MinecraftLauncher::LaunchMinecraft(
            g_config.storagePath,
            g_activeVersion,
            g_config.loader,
            g_config.nickname,
            g_config.memoryMb,
            javaPath,
            g_config.jvmPreset,
            g_config.customJvmArgs,
            callbacks,
            &g_hGameProcess,
            containerDir
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

        // Apply Windows 10/11 Dark Titlebar (Eliminates white window caption border)
        ApplyDwmDarkTitleBar(hwnd);

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
            252, 680 - 84 + 38, 132, 24,
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
            220 + 24 + 36, 54 + 16 + 64, 500, 22,
            hwnd, (HMENU)IDC_EDIT_MODSEARCH, g_hInstance, NULL
        );
        SendMessageW(g_hEditModSearch, WM_SETFONT, (WPARAM)hFontNick, TRUE);

        // 3. Custom RAM Numeric Input Box (Settings Tab)
        g_hEditRam = CreateWindowExW(
            0, L"EDIT", std::to_wstring(g_config.memoryMb).c_str(),
            WS_CHILD | ES_NUMBER | ES_AUTOHSCROLL,
            220 + 24 + 126, 54 + 16 + 62 + 38, 80, 22,
            hwnd, (HMENU)IDC_EDIT_RAM, g_hInstance, NULL
        );
        SendMessageW(g_hEditRam, WM_SETFONT, (WPARAM)hFontNick, TRUE);

        // 4. Custom JVM Arguments Edit Box (Settings Tab)
        g_hEditJvmArgs = CreateWindowExW(
            0, L"EDIT", g_config.customJvmArgs.c_str(),
            WS_CHILD | ES_AUTOHSCROLL,
            220 + 24 + 16, 54 + 16 + 242, 500, 22,
            hwnd, (HMENU)IDC_EDIT_JVMARGS, g_hInstance, NULL
        );
        SendMessageW(g_hEditJvmArgs, WM_SETFONT, (WPARAM)hFontNick, TRUE);

        // 5. Custom Java Path Edit Box (Settings Tab)
        std::wstring initialJavaPath = g_config.customJavaPath.empty() ? g_detectedJava.path : g_config.customJavaPath;
        g_hEditJavaPath = CreateWindowExW(
            0, L"EDIT", initialJavaPath.c_str(),
            WS_CHILD | ES_AUTOHSCROLL,
            220 + 24 + 16, 54 + 16 + 296, 320, 22,
            hwnd, (HMENU)IDC_EDIT_JAVAPATH, g_hInstance, NULL
        );
        SendMessageW(g_hEditJavaPath, WM_SETFONT, (WPARAM)hFontNick, TRUE);

        // 6. Game Resolution Inputs (Settings Tab)
        g_hEditGameWidth = CreateWindowExW(
            0, L"EDIT", std::to_wstring(g_config.windowWidth).c_str(),
            WS_CHILD | ES_NUMBER | ES_AUTOHSCROLL,
            220 + 24 + 140, 54 + 16 + 394, 64, 22,
            hwnd, (HMENU)IDC_EDIT_GAMEWIDTH, g_hInstance, NULL
        );
        SendMessageW(g_hEditGameWidth, WM_SETFONT, (WPARAM)hFontNick, TRUE);

        g_config.customJavaPath = MinecraftLauncher::CleanPath(g_config.customJavaPath);
        g_detectedJava = MinecraftLauncher::DetectJava(g_config.customJavaPath);
        g_detectedJava.path = MinecraftLauncher::CleanPath(g_detectedJava.path);
        EnsureContainerStructure(g_config.storagePath, g_containers[0].id);
        SyncInstalledModsWithDisk(g_config.storagePath, g_containers[0].id);

        g_hEditGameHeight = CreateWindowExW(
            0, L"EDIT", std::to_wstring(g_config.windowHeight).c_str(),
            WS_CHILD | ES_NUMBER | ES_AUTOHSCROLL,
            220 + 24 + 224, 54 + 16 + 394, 64, 22,
            hwnd, (HMENU)IDC_EDIT_GAMEHEIGHT, g_hInstance, NULL
        );
        SendMessageW(g_hEditGameHeight, WM_SETFONT, (WPARAM)hFontNick, TRUE);

        // The catalog starts with one lightweight 20-item Modrinth batch.
        // More results are fetched only when the user reaches the end.
        ResetModrinthCatalog(L"", hwnd);
        PostMessageW(hwnd, WM_SIZE, 0, 0);

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
                g_tabScrollOffset = 0;
                // Installed view filters real filenames locally. The online
                // catalog queries Modrinth by name once a useful term is typed.
                if (g_showInstalledMods) {
                    PostMessageW(hwnd, WM_SIZE, 0, 0);
                } else if (wcslen(buf) == 0 || wcslen(buf) >= 2) {
                    ResetModrinthCatalog(g_modSearchQuery, hwnd);
                } else {
                    FilterMods();
                }
                InvalidateRect(hwnd, NULL, FALSE);
            }
            else if (id == IDC_EDIT_RAM && g_hEditRam && !g_isDraggingSlider && !g_suppressRamChange) {
                WCHAR buf[32] = { 0 };
                GetWindowTextW(g_hEditRam, buf, 31);
                if (wcslen(buf) > 0) {
                    int val = _wtoi(buf);
                    if (val >= 512 && val <= 32768) {
                        SetGlobalRam(val);
                        InvalidateRect(hwnd, NULL, FALSE);
                    }
                }
            }
            else if (id == IDC_EDIT_JVMARGS && g_hEditJvmArgs && !g_suppressJvmChange) {
                WCHAR buf[1024] = { 0 };
                GetWindowTextW(g_hEditJvmArgs, buf, 1023);
                g_config.customJvmArgs = buf;
                g_config.jvmPreset = L"custom";
                SaveConfig(g_config);
            }
            else if (id == IDC_EDIT_JAVAPATH && g_hEditJavaPath && !g_suppressJavaPathChange) {
                WCHAR buf[512] = { 0 };
                GetWindowTextW(g_hEditJavaPath, buf, 511);
                g_config.customJavaPath = buf;
                SaveConfig(g_config);
            }
            else if (id == IDC_EDIT_GAMEWIDTH && g_hEditGameWidth) {
                WCHAR buf[32] = { 0 };
                GetWindowTextW(g_hEditGameWidth, buf, 31);
                if (wcslen(buf) > 0) {
                    g_config.windowWidth = _wtoi(buf);
                    SaveConfig(g_config);
                }
            }
            else if (id == IDC_EDIT_GAMEHEIGHT && g_hEditGameHeight) {
                WCHAR buf[32] = { 0 };
                GetWindowTextW(g_hEditGameHeight, buf, 31);
                if (wcslen(buf) > 0) {
                    g_config.windowHeight = _wtoi(buf);
                    SaveConfig(g_config);
                }
            }
        }
        return 0;
    }

    case WM_CTLCOLOREDIT: {
        HDC hdcEdit = (HDC)wParam;
        SetTextColor(hdcEdit, RGB(240, 242, 248));
        COLORREF bgCol = RGB(g_currentTheme.bgSurfaceElevated.GetR(),
                             g_currentTheme.bgSurfaceElevated.GetG(),
                             g_currentTheme.bgSurfaceElevated.GetB());
        SetBkColor(hdcEdit, bgCol);
        static HBRUSH hEditBg = NULL;
        if (hEditBg) DeleteObject(hEditBg);
        hEditBg = CreateSolidBrush(bgCol);
        return (LRESULT)hEditBg;
    }

    case WM_SIZE: {
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);
        int width = clientRect.right - clientRect.left;
        int height = clientRect.bottom - clientRect.top;

        bool isSettings = (g_currentTab == 4);
        int contentX = 220 + 24;
        int contentW = std::max(120, width - 220 - 48);
        int setY = 54 + 16 + 62;
        int jvmCardY = setY + 118;
        int optCardY = jvmCardY + GetSettingsJvmCardHeight(contentW) + 8;
        const int scroll = g_tabScrollOffset;

        auto showInViewport = [height](HWND control, bool intended, int y, int h) {
            const bool visible = intended && y + h >= 54 && y <= height - 84;
            ShowWindow(control, visible ? SW_SHOW : SW_HIDE);
        };

        if (g_hEditNick) {
            SetWindowPos(g_hEditNick, NULL, 252, height - 84 + 38, 132, 24, SWP_NOZORDER);
        }
        if (g_hEditModSearch) {
            const int y = 54 + 16 + 64 - scroll;
            const int installedBtnW = std::min(160, std::max(132, contentW / 4));
            const int searchW = std::max(120, contentW - installedBtnW - 8);
            SetWindowPos(g_hEditModSearch, NULL, contentX + 36, y, std::max(60, searchW - 48), 22, SWP_NOZORDER);
            showInViewport(g_hEditModSearch, g_currentTab == 1 && g_selectedModDetail == nullptr, y, 22);
        }
        if (g_hEditRam) {
            const int y = setY + 36 - scroll;
            SetWindowPos(g_hEditRam, NULL, contentX + 126, y, 80, 22, SWP_NOZORDER);
            showInViewport(g_hEditRam, isSettings, y, 22);
        }
        if (g_hEditJvmArgs) {
            const int y = jvmCardY + GetSettingsJvmArgsInputOffset(contentW) + 3 - scroll;
            SetWindowPos(g_hEditJvmArgs, NULL, contentX + 18, y, std::max(80, contentW - 36), 22, SWP_NOZORDER);
            showInViewport(g_hEditJvmArgs, isSettings, y, 22);
        }
        if (g_hEditJavaPath) {
            const int y = jvmCardY + GetSettingsJavaPathInputOffset(contentW) + 3 - scroll;
            SetWindowPos(g_hEditJavaPath, NULL, contentX + 18, y, std::max(80, contentW - 224), 22, SWP_NOZORDER);
            showInViewport(g_hEditJavaPath, isSettings, y, 22);
        }
        if (g_hEditGameWidth) {
            const int y = optCardY + 30 - scroll;
            SetWindowPos(g_hEditGameWidth, NULL, contentX + 140, y, 62, 22, SWP_NOZORDER);
            showInViewport(g_hEditGameWidth, isSettings, y, 22);
        }
        if (g_hEditGameHeight) {
            const int y = optCardY + 30 - scroll;
            SetWindowPos(g_hEditGameHeight, NULL, contentX + 228, y, 62, 22, SWP_NOZORDER);
            showInViewport(g_hEditGameHeight, isSettings, y, 22);
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

    case WM_MOUSEWHEEL: {
        const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        if (g_currentTab != 2) {
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            const int height = clientRect.bottom - clientRect.top;
            const int notches = std::max(1, std::abs(delta) / WHEEL_DELTA);
            const int previousOffset = g_tabScrollOffset;
            g_tabScrollOffset += delta < 0 ? 88 * notches : -88 * notches;
            ClampTabScroll(height);

            // Infinite Modrinth catalog: fetching is tied to reaching the end
            // of the physical list, not to synthetic page buttons.
            if (g_currentTab == 1 && !g_showInstalledMods && g_selectedModDetail == nullptr &&
                g_tabScrollOffset >= GetTabScrollMax(height) - 140) {
                FetchNextModrinthCatalogBatch(hwnd);
            }
            if (previousOffset != g_tabScrollOffset) {
                PostMessageW(hwnd, WM_SIZE, 0, 0);
            }
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        std::lock_guard<std::mutex> lock(g_logsMutex);
        const int totalLogs = (int)g_logs.size();
        const int termH = 460;
        const int visibleLines = (termH - 24) / 18;
        const int maxOffset = std::max(0, totalLogs - visibleLines);
        if (g_logScrollOffset < 0) g_logScrollOffset = maxOffset;
        const int linesToScroll = (delta / WHEEL_DELTA) * 3;
        g_logScrollOffset -= linesToScroll;
        if (g_logScrollOffset < 0) g_logScrollOffset = 0;
        if (g_logScrollOffset >= maxOffset) g_logScrollOffset = -1;
        InvalidateRect(hwnd, NULL, FALSE);
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
        if (x < 220 && y >= 88 && y < 88 + 5 * 46) {
            g_hoveredTab = (y - 88) / 46;
        }

        int playBtnW = 190;
        int playBtnH = 52;
        int playBtnX = width - playBtnW - 20;
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
            mb = (mb / 128) * 128;
            if (mb < 1024) mb = 1024;
            if (mb > 16384) mb = 16384;

            if (g_config.memoryMb != mb) {
                SetGlobalRam(mb);
                if (g_hEditRam) {
                    g_suppressRamChange = true;
                    SetWindowTextW(g_hEditRam, std::to_wstring(mb).c_str());
                    g_suppressRamChange = false;
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
                g_selectedModDetail = nullptr;
                g_tabScrollOffset = 0;
                if (g_currentTab == 1) {
                    // Opening the Mods tab normally always means the catalog;
                    // installed mods remains an explicit adjacent button.
                    g_showInstalledMods = false;
                    g_selectedCategory = L"Все";
                    FilterMods();
                }

                bool isSet = (g_currentTab == 4);
                if (g_hEditModSearch) ShowWindow(g_hEditModSearch, (g_currentTab == 1 && g_selectedModDetail == nullptr) ? SW_SHOW : SW_HIDE);
                if (g_hEditRam) ShowWindow(g_hEditRam, isSet ? SW_SHOW : SW_HIDE);
                if (g_hEditJvmArgs) ShowWindow(g_hEditJvmArgs, isSet ? SW_SHOW : SW_HIDE);
                if (g_hEditJavaPath) ShowWindow(g_hEditJavaPath, isSet ? SW_SHOW : SW_HIDE);
                if (g_hEditGameWidth) ShowWindow(g_hEditGameWidth, isSet ? SW_SHOW : SW_HIDE);
                if (g_hEditGameHeight) ShowWindow(g_hEditGameHeight, isSet ? SW_SHOW : SW_HIDE);

                PostMessageW(hwnd, WM_SIZE, 0, 0);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        // ====================================================================
        // TOP BAR CONTAINER SELECTOR DROPDOWN MENU
        // ====================================================================
        if (x >= 220 + 20 && x <= 220 + 300 && y >= 11 && y <= 43) {
            HMENU hMenu = CreatePopupMenu();
            for (size_t i = 0; i < g_containers.size(); ++i) {
                UINT flags = MF_STRING;
                if ((int)i == g_activeContainerIndex) flags |= MF_CHECKED;
                std::wstring itemText = g_containers[i].name + L" [" + g_containers[i].version + L" / " + g_containers[i].loader + L"]";
                AppendMenuW(hMenu, flags, 3000 + (UINT)i, itemText.c_str());
            }
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, 3999, Tr(L"+ Создать новый контейнер...", L"+ Create New Container..."));

            POINT pt = { 220 + 20, 11 + 34 };
            ClientToScreen(hwnd, &pt);
            int cmd = TrackPopupMenuEx(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD, pt.x, pt.y, hwnd, NULL);
            DestroyMenu(hMenu);

            if (cmd >= 3000 && cmd < 3000 + (int)g_containers.size()) {
                SetActiveContainer(cmd - 3000);
            } else if (cmd == 3999) {
                CreateNewContainerDialog();
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

        // Click Play / Stop button
        int playBtnW = 190;
        int playBtnH = 52;
        int playBtnX = width - playBtnW - 20;
        int playBtnY = height - 84 + 16;
        if (x >= playBtnX && x <= playBtnX + playBtnW && y >= playBtnY && y <= playBtnY + playBtnH) {
            if (g_isGameRunning.load()) {
                AppendLog(Tr(L"[Aether] Остановка процесса Minecraft по запросу игрока...",
                             L"[Aether] Terminating Minecraft process on player request..."));
                if (g_hGameProcess && g_hGameProcess != INVALID_HANDLE_VALUE) {
                    MinecraftLauncher::TerminateRunningGame(g_hGameProcess);
                    g_hGameProcess = NULL;
                }
                g_isGameRunning.store(false);
                g_launchStatus = Tr(L"Игра остановлена", L"Game stopped");
                AppendLog(Tr(L"[Aether] Игра успешно остановлена.", L"[Aether] Game successfully stopped."));
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            g_pressPlayBtn = true;
            InvalidateRect(hwnd, NULL, FALSE);
            StartMinecraftLaunch();
            return 0;
        }

        // ====================================================================
        // VERSION SELECTOR DROPDOWN MENU (21 Versions!)
        // ====================================================================
        if (x >= 400 && x <= 505 && y >= height - 84 + 34 && y <= height - 84 + 66) {
            HMENU hMenu = CreatePopupMenu();
            for (size_t i = 0; i < g_versions.size(); ++i) {
                UINT flags = MF_STRING;
                if (g_activeVersion == g_versions[i]) flags |= MF_CHECKED;
                AppendMenuW(hMenu, flags, 2000 + (UINT)i, g_versions[i].c_str());
            }

            POINT pt = { 400, height - 84 + 34 };
            ClientToScreen(hwnd, &pt);
            int cmd = TrackPopupMenuEx(hMenu, TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_RETURNCMD, pt.x, pt.y, hwnd, NULL);
            DestroyMenu(hMenu);

            if (cmd >= 2000 && cmd < 2000 + (int)g_versions.size()) {
                int selIdx = cmd - 2000;
                g_selectedVersionIndex = selIdx;
                g_activeVersion = g_versions[selIdx];
                g_containers[g_activeContainerIndex].version = g_activeVersion;
                // Keep a custom container name intact when its version changes.
                g_activeContainerName = g_containers[g_activeContainerIndex].name;
                g_config.version = g_activeVersion;
                SaveConfig(g_config);
                SaveContainerProfiles();
                ResetModrinthCatalog(g_modSearchQuery, hwnd);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        // ====================================================================
        // MOD LOADER SELECTOR DROPDOWN MENU (Dedicated on Bottom Dock!)
        // ====================================================================
        if (x >= 518 && x <= 638 && y >= height - 84 + 34 && y <= height - 84 + 66) {
            HMENU hMenu = CreatePopupMenu();
            for (size_t i = 0; i < g_loaders.size(); ++i) {
                UINT flags = MF_STRING;
                if (g_config.loader == g_loaders[i].first) flags |= MF_CHECKED;
                AppendMenuW(hMenu, flags, 2500 + (UINT)i, g_loaders[i].second.c_str());
            }

            POINT pt = { 518, height - 84 + 34 };
            ClientToScreen(hwnd, &pt);
            int cmd = TrackPopupMenuEx(hMenu, TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_RETURNCMD, pt.x, pt.y, hwnd, NULL);
            DestroyMenu(hMenu);

            if (cmd >= 2500 && cmd < 2500 + (int)g_loaders.size()) {
                int lIdx = cmd - 2500;
                g_config.loader = g_loaders[lIdx].first;
                g_containers[g_activeContainerIndex].loader = g_config.loader;

                // Synchronize loader filter
                if (g_config.loader == L"fabric") g_selectedLoaderFilter = L"Fabric";
                else if (g_config.loader == L"forge") g_selectedLoaderFilter = L"Forge";
                else if (g_config.loader == L"neoforge") g_selectedLoaderFilter = L"NeoForge";
                else if (g_config.loader == L"quilt") g_selectedLoaderFilter = L"Quilt";
                else g_selectedLoaderFilter = L"Все";

                SaveConfig(g_config);
                SaveContainerProfiles();
                AppendLog(Tr(L"[Aether] Выбран загрузчик модов: " + g_loaders[lIdx].second,
                             L"[Aether] Selected mod loader: " + g_loaders[lIdx].second));
                ResetModrinthCatalog(g_modSearchQuery, hwnd);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        // --- Tab 0: Containers Tab Interaction ---
        if (g_currentTab == 0) {
            int contentX = 220 + 24;
            int contentW = width - 220 - 48;
            int contentY = 54 + 16;
            const int tabY = y + g_tabScrollOffset;

            int addBtnW = 180;
            int addBtnX = contentX + contentW - addBtnW;
            if (x >= addBtnX && x <= addBtnX + addBtnW && tabY >= contentY && tabY <= contentY + 34) {
                CreateNewContainerDialog();
                return 0;
            }

            int cardY = contentY + 68;
            int cardW = contentW;
            int cardH = 120;

            for (size_t i = 0; i < g_containers.size(); ++i) {
                int cY = cardY + (int)i * (cardH + 16);
                int btnW = 160;
                int btnX = contentX + cardW - btnW - 16;
                int actBtnY = cY + 14;
                int modsBtnY = cY + 48;
                int folderBtnY = cY + 82;

                // Click Rename Button or Click Name Title -> Prompt to rename container!
                int renBtnX = contentX + 70;
                int renBtnY = cY + 92;
                int renBtnW = 125;
                if ((x >= renBtnX && x <= renBtnX + renBtnW && tabY >= renBtnY && tabY <= renBtnY + 20) ||
                    (x >= contentX + 70 && x <= contentX + 380 && tabY >= cY + 14 && tabY <= cY + 38)) {
                    std::wstring newName;
                    if (PromptTextInput(hwnd, Tr(L"Переименование контейнера", L"Rename Container"),
                                        Tr(L"Введите новое название для этого контейнера:", L"Enter a new name for this container:"),
                                        g_containers[i].name, newName)) {
                        g_containers[i].name = newName;
                        if ((int)i == g_activeContainerIndex) {
                            g_activeContainerName = newName;
                        }
                        SaveConfig(g_config);
                        SaveContainerProfiles();
                        AppendLog(Tr(L"[Aether] Название контейнера изменено на: " + newName,
                                     L"[Aether] Container name updated to: " + newName));
                        InvalidateRect(hwnd, NULL, FALSE);
                    }
                    return 0;
                }

                // Click Active / Select Container
                if (x >= btnX && x <= btnX + btnW && tabY >= actBtnY && tabY <= actBtnY + 28) {
                    SetActiveContainer((int)i);
                    return 0;
                }

                // Click Installed Mods Button -> Switch to Mods tab with "Установленные" filter!
                if (x >= btnX && x <= btnX + btnW && tabY >= modsBtnY && tabY <= modsBtnY + 28) {
                    SetActiveContainer((int)i);
                    SyncInstalledModsWithDisk(g_config.storagePath, g_containers[i].id);
                    g_currentTab = 1;
                    g_selectedCategory = L"Все";
                    g_showInstalledMods = true;
                    g_tabScrollOffset = 0;
                    if (g_hEditModSearch) ShowWindow(g_hEditModSearch, SW_SHOW);
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }

                // Click Game Folder Button -> Open Explorer at instances\<containerId> (isolated container folder)
                if (x >= btnX && x <= btnX + btnW && tabY >= folderBtnY && tabY <= folderBtnY + 28) {
                    std::wstring instDir = GetContainerDir(g_config.storagePath, g_containers[i].id);
                    EnsureContainerStructure(g_config.storagePath, g_containers[i].id);
                    ShellExecuteW(NULL, L"open", instDir.c_str(), NULL, NULL, SW_SHOW);
                    AppendLog(Tr(L"[Aether] Открыта изолированная папка контейнера: " + instDir,
                                 L"[Aether] Opened container folder: " + instDir));
                    return 0;
                }

                // Click card body
                if (x >= contentX && x <= contentX + cardW && tabY >= cY && tabY <= cY + cardH) {
                    SetActiveContainer((int)i);
                    return 0;
                }
            }
        }

        // --- Tab 1: Mods Tab Interaction ---
        if (g_currentTab == 1) {
            const int contentX = 220 + 24;
            const int contentW = width - 220 - 48;
            const int tabY = y + g_tabScrollOffset;

            if (g_selectedModDetail != nullptr) {
                if (x >= contentX && x <= contentX + 150 && tabY >= 54 + 16 && tabY <= 54 + 16 + 30) {
                    g_selectedModDetail = nullptr;
                    g_tabScrollOffset = 0;
                    PostMessageW(hwnd, WM_SIZE, 0, 0);
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }

                const int cardY = 54 + 16 + 44;
                const int mainH = (rc.bottom - rc.top) - 84;
                const int detailH = mainH - cardY - 10;
                const int actY = cardY + detailH - 52;

                if (x >= contentX + 24 && x <= contentX + 184 && tabY >= actY && tabY <= actY + 36) {
                    const std::wstring slug = g_selectedModDetail->id;
                    const std::wstring name = g_selectedModDetail->name;
                    if (g_selectedModDetail->installed) TriggerModRemove(slug, name, hwnd);
                    else TriggerModDownload(slug, name, hwnd);
                    return 0;
                }
                if (x >= contentX + 196 && x <= contentX + 406 && tabY >= actY && tabY <= actY + 36) {
                    ShellExecuteW(NULL, L"open", g_selectedModDetail->url.c_str(), NULL, NULL, SW_SHOWNORMAL);
                    return 0;
                }
                return 0;
            }

            const int searchY = 54 + 16 + 58;
            const int installedBtnW = std::min(160, std::max(132, contentW / 4));
            const int searchW = std::max(120, contentW - installedBtnW - 8);
            const int installedBtnX = contentX + searchW + 8;
            if (x >= installedBtnX && x <= installedBtnX + installedBtnW &&
                tabY >= searchY && tabY <= searchY + 34) {
                g_showInstalledMods = !g_showInstalledMods;
                g_selectedCategory = L"Все";
                g_tabScrollOffset = 0;
                if (g_activeContainerIndex >= 0 && g_activeContainerIndex < (int)g_containers.size()) {
                    SyncInstalledModsWithDisk(g_config.storagePath, g_containers[g_activeContainerIndex].id);
                }
                PostMessageW(hwnd, WM_SIZE, 0, 0);
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }

            // Disk-backed installed cards are informational: they cannot be
            // mistaken for online catalog entries or trigger a synthetic state.
            if (g_showInstalledMods) return 0;

            const int modListY = searchY + 52;
            const int modItemH = 72;
            for (int i = 0; i < (int)g_filteredMods.size(); ++i) {
                const int mY = modListY + i * (modItemH + 8);
                const int btnW = 120;
                const int btnX = contentX + contentW - btnW - 12;
                const int detBtnW = 86;
                const int detBtnX = btnX - detBtnW - 8;

                if (x >= btnX && x <= btnX + btnW && tabY >= mY + 22 && tabY <= mY + 50) {
                    const std::wstring slug = g_filteredMods[i].id;
                    const std::wstring name = g_filteredMods[i].name;
                    if (g_filteredMods[i].installed) TriggerModRemove(slug, name, hwnd);
                    else TriggerModDownload(slug, name, hwnd);
                    return 0;
                }
                if ((x >= detBtnX && x <= detBtnX + detBtnW && tabY >= mY + 22 && tabY <= mY + 50) ||
                    (x >= contentX && x < detBtnX && tabY >= mY && tabY <= mY + modItemH)) {
                    // Detail state points to the stable backing catalog, not the
                    // temporary filtered vector that is rebuilt after each batch.
                    g_selectedModDetail = nullptr;
                    for (const ModItem& catalogMod : g_allMods) {
                        if (catalogMod.id == g_filteredMods[i].id) {
                            g_selectedModDetail = &catalogMod;
                            break;
                        }
                    }
                    if (g_hEditModSearch) ShowWindow(g_hEditModSearch, SW_HIDE);
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
            }
            return 0;
        }

        
        // --- Tab 2: Logs Tab Interaction (Download & Copy Logs) ---
        if (g_currentTab == 2) {
            int contentX = 220 + 24;
            int contentW = width - 220 - 48;
            int contentY = 54 + 16;
            int saveBtnW = 140;
            int copyBtnW = 110;
            int btnGap = 8;
            int copyBtnX = contentX + contentW - copyBtnW;
            int saveBtnX = copyBtnX - saveBtnW - btnGap;
            int logBtnY = contentY + 12;

            if (x >= saveBtnX && x <= saveBtnX + saveBtnW && y >= logBtnY && y <= logBtnY + 28) {
                SaveLogsToFile(hwnd);
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }

            if (x >= copyBtnX && x <= copyBtnX + copyBtnW && y >= logBtnY && y <= logBtnY + 28) {
                CopyLogsToClipboard(hwnd);
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }

            int logY = contentY + 60;
            if (!g_lastCrashCause.empty()) logY += 88;
            int termH = (rc.bottom - rc.top - 84) - logY - 14;
            int downBtnX = contentX + contentW - 100;
            int downBtnY = logY + termH - 34;
            if (x >= downBtnX && x <= downBtnX + 85 && y >= downBtnY && y <= downBtnY + 24) {
                g_logScrollOffset = -1;
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
        }

        // --- Tab 4: Settings Tab (RAM Slider Drag & Presets - Global RAM!) ---
        if (g_currentTab == 4) {
            int contentX = 220 + 24;
            int contentW = width - 220 - 48;
            int contentY = 54 + 16;
            const int tabY = y + g_tabScrollOffset;
            int setY = contentY + 62;

            int sliderX = contentX + 16;
            int sliderY = setY + 68;
            int sliderW = contentW - 32;

            // RAM Slider Click and Drag Hit Test
            if (x >= sliderX - 10 && x <= sliderX + sliderW + 10 && tabY >= sliderY - 14 && tabY <= sliderY + 14) {
                g_isDraggingSlider = true;
                SetCapture(hwnd);

                float pct = (float)(x - sliderX) / (float)sliderW;
                if (pct < 0.0f) pct = 0.0f;
                if (pct > 1.0f) pct = 1.0f;

                int mb = 1024 + (int)(pct * (16384 - 1024));
                mb = (mb / 128) * 128;
                if (mb < 1024) mb = 1024;
                if (mb > 16384) mb = 16384;

                SetGlobalRam(mb);
                if (g_hEditRam) {
                    g_suppressRamChange = true;
                    SetWindowTextW(g_hEditRam, std::to_wstring(mb).c_str());
                    g_suppressRamChange = false;
                }
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }

            // RAM Preset Pills Hit Test (setY + 82, height 22)
            int pillStartX = contentX + 16;
            int pillY = setY + 82;
            for (size_t i = 0; i < g_ramPresets.size(); ++i) {
                int pX = pillStartX + (int)i * 72;
                if (x >= pX && x <= pX + 62 && tabY >= pillY && tabY <= pillY + 22) {
                    SetGlobalRam(g_ramPresets[i]);
                    if (g_hEditRam) {
                        g_suppressRamChange = true;
                        SetWindowTextW(g_hEditRam, std::to_wstring(g_config.memoryMb).c_str());
                        g_suppressRamChange = false;
                    }
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
            }

            int jvmCardY = setY + 118;

            // Click GC Preset Pills
            struct GcPresetItem { std::wstring id; std::wstring label; const wchar_t* flags; };
            const std::vector<std::wstring> gcLabels = GetSettingsGcPresetLabels();
            std::vector<GcPresetItem> gcPresets = {
                { L"aikar", gcLabels[0], PRESET_AIKAR },
                { L"zgc", gcLabels[1], PRESET_ZGC },
                { L"shenandoah", gcLabels[2], PRESET_SHENANDOAH },
                { L"standard", gcLabels[3], PRESET_G1GC },
                { L"custom", gcLabels[4], L"" }
            };

            int gcPillX = contentX + 16;
            int gcPillY = jvmCardY + 54;
            const int gcPillRight = contentX + contentW - 16;
            for (const auto& pr : gcPresets) {
                int pillW = GetSettingsGcPillWidth(pr.label);
                if (gcPillX + pillW > gcPillRight && gcPillX != contentX + 16) {
                    gcPillX = contentX + 16;
                    gcPillY += 30;
                }
                if (x >= gcPillX && x <= gcPillX + pillW && tabY >= gcPillY && tabY <= gcPillY + 24) {
                    g_config.jvmPreset = pr.id;
                    if (pr.id != L"custom") {
                        g_config.customJvmArgs = pr.flags;
                        if (g_hEditJvmArgs) {
                            g_suppressJvmChange = true;
                            SetWindowTextW(g_hEditJvmArgs, g_config.customJvmArgs.c_str());
                            g_suppressJvmChange = false;
                        }
                    }
                    SaveConfig(g_config);
                    AppendLog(Tr(L"[Aether] Выбран пресет сборщика мусора: " + pr.label,
                                 L"[Aether] Selected GC preset: " + pr.label));
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
                gcPillX += pillW + 6;
            }

            // Click Auto-detect Java Button (Updates text box with detected Java path)
            int autoBtnX = contentX + contentW - 192;
            if (x >= autoBtnX && x <= autoBtnX + 88 && tabY >= jvmCardY + GetSettingsJavaPathInputOffset(contentW) &&
                tabY <= jvmCardY + GetSettingsJavaPathInputOffset(contentW) + 28) {
                g_detectedJava = MinecraftLauncher::DetectJava(L"");
                g_config.customJavaPath = g_detectedJava.path;
                if (g_hEditJavaPath) {
                    g_suppressJavaPathChange = true;
                    SetWindowTextW(g_hEditJavaPath, g_detectedJava.path.c_str());
                    g_suppressJavaPathChange = false;
                }
                SaveConfig(g_config);
                AppendLog(Tr(L"[Aether] Автоопределение Java: " + g_detectedJava.path + L" (Java " + std::to_wstring(g_detectedJava.majorVersion) + L")",
                             L"[Aether] Auto-detected Java: " + g_detectedJava.path));
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }

            // Click Test Java Button
            int testBtnX = contentX + contentW - 98;
            if (x >= testBtnX && x <= testBtnX + 88 && tabY >= jvmCardY + GetSettingsJavaPathInputOffset(contentW) &&
                tabY <= jvmCardY + GetSettingsJavaPathInputOffset(contentW) + 28) {
                WCHAR jBuf[512] = { 0 };
                if (g_hEditJavaPath) GetWindowTextW(g_hEditJavaPath, jBuf, 511);
                g_detectedJava = MinecraftLauncher::DetectJava(jBuf);
                AppendLog(Tr(L"[JavaDetector] Тестирование среды Java... Обнаружена: Java " + std::to_wstring(g_detectedJava.majorVersion) +
                             L" (" + g_detectedJava.vendor + L") по пути: " + g_detectedJava.path,
                             L"[JavaDetector] Testing Java... Found Java " + std::to_wstring(g_detectedJava.majorVersion) +
                             L" (" + g_detectedJava.vendor + L") at: " + g_detectedJava.path));
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }

            // Click Fullscreen & Low CPU Mode Toggles. Mirror the responsive
            // positions used by PaintMainUI instead of retaining off-screen hitboxes.
            const int optCardY = jvmCardY + GetSettingsJvmCardHeight(contentW) + 8;
            const bool compactOptions = contentW < 680;
            const int fsBtnX = compactOptions ? contentX + 16 : contentX + 310;
            const int fsBtnY = compactOptions ? optCardY + 62 : optCardY + 26;
            const bool stackCompactToggles = contentW < 390;
            const int cpuBtnX = compactOptions ? (stackCompactToggles ? fsBtnX : fsBtnX + 168) : contentX + 480;
            const int cpuBtnY = stackCompactToggles ? fsBtnY + 34 : fsBtnY;
            if (x >= fsBtnX && x <= fsBtnX + 160 && tabY >= fsBtnY && tabY <= fsBtnY + 28) {
                g_config.fullscreen = !g_config.fullscreen;
                SaveConfig(g_config);
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            if (x >= cpuBtnX && x <= cpuBtnX + 175 && tabY >= cpuBtnY && tabY <= cpuBtnY + 28) {
                g_config.lowCpuMode = !g_config.lowCpuMode;
                SaveConfig(g_config);
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }

            // Click wrapped theme pills.
            const int themeY = optCardY + GetSettingsOptionsCardHeight(contentW) + 8;
            const int themePillW = 114;
            const int themeRight = contentX + contentW - 16;
            int tX = contentX + 16;
            int tY = themeY + 32;
            for (size_t i = 0; i < g_themes.size(); ++i) {
                if (tX + themePillW > themeRight && tX != contentX + 16) {
                    tX = contentX + 16;
                    tY += 34;
                }
                if (x >= tX && x <= tX + themePillW && tabY >= tY && tabY <= tY + 26) {
                    g_currentTheme = g_themes[i];
                    g_config.theme = g_themes[i].id;
                    SaveConfig(g_config);
                    ApplyDwmDarkTitleBar(hwnd);
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
                tX += themePillW + 8;
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

    case WM_AETHER_MODRINTH_BATCH: {
        ApplyModrinthCatalogBatch(reinterpret_cast<ModrinthCatalogBatch*>(lParam), hwnd);
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

    // Restore the full profile list, including the user-provided container names.
    // Old config files are migrated to the single default profile on first run.
    LoadContainerProfiles();
    g_activeContainerName = g_containers[g_activeContainerIndex].name;
    g_activeVersion = g_containers[g_activeContainerIndex].version;
    g_config.version = g_activeVersion;
    g_config.loader = g_containers[g_activeContainerIndex].loader;
    g_config.memoryMb = g_containers[g_activeContainerIndex].memoryMb;
    SaveConfig(g_config);
    SaveContainerProfiles();
    // Catalog cards keep stable addresses while background batches append.
    g_allMods.reserve(10000);

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
        L"Aether Launcher",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
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
