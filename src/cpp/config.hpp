#pragma once
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shlobj.h>
#include <string>
#include <vector>

struct LauncherConfig {
    std::wstring nickname = L"Player";
    std::wstring version = L"1.21.1";
    std::wstring loader = L"vanilla";
    int memoryMb = 4096;
    std::wstring theme = L"void-purple"; // void-purple, crimson, cyan-neon, emerald
    std::wstring customJavaPath = L"";
    std::wstring jvmPreset = L"aikar";
    std::wstring customJvmArgs = L"-XX:+UseG1GC -XX:+ParallelRefProcEnabled -XX:MaxGCPauseMillis=200 -XX:+UnlockExperimentalVMOptions -XX:+DisableExplicitGC -XX:+AlwaysPreTouch";
    int windowWidth = 1280;
    int windowHeight = 720;
    bool fullscreen = false;
    bool lowCpuMode = true;
    // Offline is a local nickname session. Microsoft uses an encrypted refresh
    // token and is only valid after Minecraft ownership/profile verification.
    std::wstring accountMode = L"offline";
    std::wstring microsoftRefreshTokenProtected = L"";
    std::wstring microsoftPlayerName = L"";
    std::wstring microsoftPlayerUuid = L"";
    std::wstring storagePath = L"";
};

inline std::wstring GetDefaultStoragePath() {
    WCHAR appData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appData))) {
        return std::wstring(appData) + L"\\AetherLauncher";
    }
    return L"C:\\AetherLauncher";
}

inline void EnsureDirectoryExists(const std::wstring& path) {
    WCHAR temp[MAX_PATH];
    wcsncpy(temp, path.c_str(), MAX_PATH);
    for (WCHAR* p = temp + 1; *p; p++) {
        if (*p == L'\\' || *p == L'/') {
            WCHAR orig = *p;
            *p = L'\0';
            CreateDirectoryW(temp, NULL);
            *p = orig;
        }
    }
    CreateDirectoryW(temp, NULL);
}

inline LauncherConfig LoadConfig() {
    LauncherConfig cfg;
    cfg.storagePath = GetDefaultStoragePath();
    EnsureDirectoryExists(cfg.storagePath);

    std::wstring iniFile = cfg.storagePath + L"\\config.ini";

    WCHAR buf[4096];
    GetPrivateProfileStringW(L"Aether", L"Nickname", L"Player", buf, 4096, iniFile.c_str());
    cfg.nickname = buf;

    GetPrivateProfileStringW(L"Aether", L"Version", L"1.21.1", buf, 4096, iniFile.c_str());
    cfg.version = buf;

    GetPrivateProfileStringW(L"Aether", L"Loader", L"vanilla", buf, 4096, iniFile.c_str());
    cfg.loader = buf;

    cfg.memoryMb = GetPrivateProfileIntW(L"Aether", L"MemoryMb", 4096, iniFile.c_str());

    GetPrivateProfileStringW(L"Aether", L"Theme", L"void-purple", buf, 4096, iniFile.c_str());
    cfg.theme = buf;

    GetPrivateProfileStringW(L"Aether", L"JavaPath", L"", buf, 4096, iniFile.c_str());
    cfg.customJavaPath = buf;

    GetPrivateProfileStringW(L"Aether", L"JvmPreset", L"aikar", buf, 4096, iniFile.c_str());
    cfg.jvmPreset = buf;

    GetPrivateProfileStringW(L"Aether", L"CustomJvmArgs",
        L"-XX:+UseG1GC -XX:+ParallelRefProcEnabled -XX:MaxGCPauseMillis=200 -XX:+UnlockExperimentalVMOptions -XX:+DisableExplicitGC -XX:+AlwaysPreTouch",
        buf, 4096, iniFile.c_str());
    cfg.customJvmArgs = buf;

    cfg.windowWidth = GetPrivateProfileIntW(L"Aether", L"WindowWidth", 1280, iniFile.c_str());
    cfg.windowHeight = GetPrivateProfileIntW(L"Aether", L"WindowHeight", 720, iniFile.c_str());
    cfg.fullscreen = GetPrivateProfileIntW(L"Aether", L"Fullscreen", 0, iniFile.c_str()) != 0;
    cfg.lowCpuMode = GetPrivateProfileIntW(L"Aether", L"LowCpuMode", 1, iniFile.c_str()) != 0;
    GetPrivateProfileStringW(L"Aether", L"AccountMode", L"offline", buf, 4096, iniFile.c_str());
    cfg.accountMode = buf;
    GetPrivateProfileStringW(L"Aether", L"MicrosoftRefreshToken", L"", buf, 4096, iniFile.c_str());
    cfg.microsoftRefreshTokenProtected = buf;
    GetPrivateProfileStringW(L"Aether", L"MicrosoftPlayerName", L"", buf, 4096, iniFile.c_str());
    cfg.microsoftPlayerName = buf;
    GetPrivateProfileStringW(L"Aether", L"MicrosoftPlayerUuid", L"", buf, 4096, iniFile.c_str());
    cfg.microsoftPlayerUuid = buf;

    return cfg;
}

inline void SaveConfig(const LauncherConfig& cfg) {
    EnsureDirectoryExists(cfg.storagePath);
    std::wstring iniFile = cfg.storagePath + L"\\config.ini";

    WritePrivateProfileStringW(L"Aether", L"Nickname", cfg.nickname.c_str(), iniFile.c_str());
    WritePrivateProfileStringW(L"Aether", L"Version", cfg.version.c_str(), iniFile.c_str());
    WritePrivateProfileStringW(L"Aether", L"Loader", cfg.loader.c_str(), iniFile.c_str());
    WritePrivateProfileStringW(L"Aether", L"MemoryMb", std::to_wstring(cfg.memoryMb).c_str(), iniFile.c_str());
    WritePrivateProfileStringW(L"Aether", L"Theme", cfg.theme.c_str(), iniFile.c_str());
    WritePrivateProfileStringW(L"Aether", L"JavaPath", cfg.customJavaPath.c_str(), iniFile.c_str());
    WritePrivateProfileStringW(L"Aether", L"JvmPreset", cfg.jvmPreset.c_str(), iniFile.c_str());
    WritePrivateProfileStringW(L"Aether", L"CustomJvmArgs", cfg.customJvmArgs.c_str(), iniFile.c_str());
    WritePrivateProfileStringW(L"Aether", L"WindowWidth", std::to_wstring(cfg.windowWidth).c_str(), iniFile.c_str());
    WritePrivateProfileStringW(L"Aether", L"WindowHeight", std::to_wstring(cfg.windowHeight).c_str(), iniFile.c_str());
    WritePrivateProfileStringW(L"Aether", L"Fullscreen", cfg.fullscreen ? L"1" : L"0", iniFile.c_str());
    WritePrivateProfileStringW(L"Aether", L"LowCpuMode", cfg.lowCpuMode ? L"1" : L"0", iniFile.c_str());
    WritePrivateProfileStringW(L"Aether", L"AccountMode", cfg.accountMode.c_str(), iniFile.c_str());
    WritePrivateProfileStringW(L"Aether", L"MicrosoftRefreshToken", cfg.microsoftRefreshTokenProtected.c_str(), iniFile.c_str());
    WritePrivateProfileStringW(L"Aether", L"MicrosoftPlayerName", cfg.microsoftPlayerName.c_str(), iniFile.c_str());
    WritePrivateProfileStringW(L"Aether", L"MicrosoftPlayerUuid", cfg.microsoftPlayerUuid.c_str(), iniFile.c_str());
}
