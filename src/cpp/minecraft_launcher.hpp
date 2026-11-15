#pragma once
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <wininet.h>
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <sstream>

struct JavaInfo {
    std::wstring path;
    int majorVersion = 0;
    std::wstring vendor;
    bool is64Bit = true;
};

struct LaunchProgressCallback {
    std::function<void(const std::wstring& status, int percent)> onProgress;
    std::function<void(const std::wstring& logLine, bool isError)> onLog;
    std::function<void(int exitCode)> onGameExited;
};

class MinecraftLauncher {
public:
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
        std::function<void(const std::wstring& status, int percent)> progress = nullptr
    );
    static bool LaunchMinecraft(
        const std::wstring& storageDir,
        const std::wstring& mcVersion,
        const std::wstring& loader,
        const std::wstring& nickname,
        int memoryMb,
        const std::wstring& customJava,
        const std::wstring& jvmPreset,
        const std::wstring& customArgs,
        LaunchProgressCallback callbacks,
        HANDLE* outProcessHandle
    );
    static void TerminateRunningGame(HANDLE hProcess);
    static std::wstring AnalyzeCrashLog(const std::wstring& logText);
};
