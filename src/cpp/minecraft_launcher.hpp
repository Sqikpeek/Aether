#pragma once
#include <string>
#include <vector>
#include <functional>
#include <windows.h>
#include <wininet.h>
#include <thread>
#include <atomic>

struct JavaInfo {
    std::wstring path;
    int majorVersion = 0;
    std::wstring vendor;
    bool is64Bit = true;
};

struct MinecraftAccount {
    std::wstring username;
    std::wstring uuid;
    // Minecraft Services token stays in memory; it is refreshed from the
    // DPAPI-protected Microsoft refresh token when this timestamp is near.
    std::wstring accessToken;
    long long accessTokenExpiresAt = 0;
    bool isMicrosoft = false;
};

struct LaunchProgressCallback {
    std::function<void(const std::wstring& status, int percent)> onProgress;
    std::function<void(const std::wstring& line, bool isError)> onLog;
    std::function<void(int exitCode)> onGameExited;
};

class MinecraftLauncher {
public:
    static std::wstring CleanPath(const std::wstring& path);
    static JavaInfo DetectJava(const std::wstring& customPath = L"");
    static int GetRequiredJavaVersion(const std::wstring& mcVersion);
    static bool DownloadHttpFile(
        const std::wstring& url,
        const std::wstring& destPath,
        std::function<void(int percent)> progress = nullptr
    );
    static bool PrepareMinecraftFiles(
        const std::wstring& storageDir,
        const std::wstring& mcVersion,
        const std::wstring& loader,
        std::function<void(const std::wstring& status, int percent)> progress
    );
    static bool PrepareMinecraftFiles(
        const std::wstring& storageDir,
        const std::wstring& mcVersion,
        std::function<void(const std::wstring& status, int percent)> progress = nullptr
    ) {
        return PrepareMinecraftFiles(storageDir, mcVersion, L"vanilla", progress);
    }
    static bool LaunchMinecraft(
        const std::wstring& storageDir,
        const std::wstring& mcVersion,
        const std::wstring& loader,
        const MinecraftAccount& account,
        int memoryMb,
        const std::wstring& customJava,
        const std::wstring& jvmPreset,
        const std::wstring& customArgs,
        LaunchProgressCallback callbacks,
        HANDLE* outProcessHandle = nullptr,
        const std::wstring& instanceDirOverride = L""
    );
    static void TerminateRunningGame(HANDLE hProcess);
    static std::wstring AnalyzeCrashLog(const std::wstring& logText);
};
