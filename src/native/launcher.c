#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdbool.h>

#include "embedded_dist.h"

static bool FileExists(const WCHAR* path) {
    DWORD dwAttrib = GetFileAttributesW(path);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

static void CreateDirectoryRecursive(const WCHAR* dirPath) {
    WCHAR temp[MAX_PATH];
    wcsncpy(temp, dirPath, MAX_PATH);
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

static bool ExtractEmbeddedDist(const WCHAR* targetDir) {
    for (size_t i = 0; i < NUM_EMBEDDED_FILES; i++) {
        const EmbeddedFile* ef = &EMBEDDED_FILES[i];

        WCHAR relW[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, ef->relPath, -1, relW, MAX_PATH);

        for (WCHAR* p = relW; *p; p++) {
            if (*p == L'/') *p = L'\\';
        }

        WCHAR fullPath[MAX_PATH];
        swprintf(fullPath, MAX_PATH, L"%ls\\dist\\%ls", targetDir, relW);

        WCHAR parentDir[MAX_PATH];
        wcsncpy(parentDir, fullPath, MAX_PATH);
        WCHAR* lastSlash = wcsrchr(parentDir, L'\\');
        if (lastSlash) {
            *lastSlash = L'\0';
            CreateDirectoryRecursive(parentDir);
        }

        HANDLE hFile = CreateFileW(fullPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            WriteFile(hFile, ef->data, (DWORD)ef->size, &written, NULL);
            CloseHandle(hFile);
        }
    }
    return true;
}

static bool FindNodeExecutable(const WCHAR* baseDir, WCHAR* outNodePath, DWORD outSize) {
    // 1. Check local directory
    WCHAR localNode[MAX_PATH];
    swprintf(localNode, MAX_PATH, L"%ls\\node.exe", baseDir);
    if (FileExists(localNode)) {
        wcsncpy(outNodePath, localNode, outSize);
        return true;
    }

    // 2. Check local bin/
    swprintf(localNode, MAX_PATH, L"%ls\\bin\\node.exe", baseDir);
    if (FileExists(localNode)) {
        wcsncpy(outNodePath, localNode, outSize);
        return true;
    }

    // 3. Search in system PATH
    DWORD res = SearchPathW(NULL, L"node.exe", NULL, outSize, outNodePath, NULL);
    if (res > 0 && res < outSize) {
        return true;
    }

    // 4. Common install locations
    WCHAR pfNode[MAX_PATH];
    ExpandEnvironmentStringsW(L"%ProgramFiles%\\nodejs\\node.exe", pfNode, MAX_PATH);
    if (FileExists(pfNode)) {
        wcsncpy(outNodePath, pfNode, outSize);
        return true;
    }

    ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\\Programs\\node\\node.exe", pfNode, MAX_PATH);
    if (FileExists(pfNode)) {
        wcsncpy(outNodePath, pfNode, outSize);
        return true;
    }

    return false;
}

static bool FindDesktopAppBrowser(WCHAR* outBrowserPath, DWORD outSize) {
    WCHAR path[MAX_PATH];

    // 1. Microsoft Edge (Pre-installed on all Windows 10/11)
    ExpandEnvironmentStringsW(L"%ProgramFiles(x86)%\\Microsoft\\Edge\\Application\\msedge.exe", path, MAX_PATH);
    if (FileExists(path)) { wcsncpy(outBrowserPath, path, outSize); return true; }

    ExpandEnvironmentStringsW(L"%ProgramFiles%\\Microsoft\\Edge\\Application\\msedge.exe", path, MAX_PATH);
    if (FileExists(path)) { wcsncpy(outBrowserPath, path, outSize); return true; }

    ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\\Microsoft\\Edge\\Application\\msedge.exe", path, MAX_PATH);
    if (FileExists(path)) { wcsncpy(outBrowserPath, path, outSize); return true; }

    // 2. Google Chrome
    ExpandEnvironmentStringsW(L"%ProgramFiles%\\Google\\Chrome\\Application\\chrome.exe", path, MAX_PATH);
    if (FileExists(path)) { wcsncpy(outBrowserPath, path, outSize); return true; }

    ExpandEnvironmentStringsW(L"%ProgramFiles(x86)%\\Google\\Chrome\\Application\\chrome.exe", path, MAX_PATH);
    if (FileExists(path)) { wcsncpy(outBrowserPath, path, outSize); return true; }

    ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\\Google\\Chrome\\Application\\chrome.exe", path, MAX_PATH);
    if (FileExists(path)) { wcsncpy(outBrowserPath, path, outSize); return true; }

    // 3. Brave Browser
    ExpandEnvironmentStringsW(L"%ProgramFiles%\\BraveSoftware\\Brave-Browser\\Application\\brave.exe", path, MAX_PATH);
    if (FileExists(path)) { wcsncpy(outBrowserPath, path, outSize); return true; }

    return false;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // Single instance mutex to avoid port conflicts
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"Global\\AetherLauncherSingleInstanceMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // Already running, focus existing window or notify
        HWND existingWnd = FindWindowW(NULL, L"Aether Launcher");
        if (existingWnd) {
            SetForegroundWindow(existingWnd);
            ShowWindow(existingWnd, SW_RESTORE);
        } else {
            // Open window
            WCHAR browserPath[MAX_PATH];
            if (FindDesktopAppBrowser(browserPath, MAX_PATH)) {
                WCHAR appDataDir[MAX_PATH];
                ExpandEnvironmentStringsW(L"%APPDATA%\\AetherLauncher", appDataDir, MAX_PATH);
                WCHAR appCmd[2048];
                swprintf(appCmd, 2048,
                    L"\"%ls\" --app=\"http://localhost:3000\" --window-size=1150,730 --user-data-dir=\"%ls\\app_profile\" --app-id=\"AetherLauncher\"",
                    browserPath, appDataDir
                );
                STARTUPINFOW si;
                PROCESS_INFORMATION pi;
                ZeroMemory(&si, sizeof(si));
                si.cb = sizeof(si);
                ZeroMemory(&pi, sizeof(pi));
                CreateProcessW(NULL, appCmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
            }
        }
        return 0;
    }

    WCHAR exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);

    // Get parent directory
    WCHAR baseDir[MAX_PATH];
    wcsncpy(baseDir, exePath, MAX_PATH);
    WCHAR* lastSlash = wcsrchr(baseDir, L'\\');
    if (lastSlash) {
        *lastSlash = L'\0';
    }

    // Find Node.js runtime
    WCHAR nodePath[MAX_PATH];
    if (!FindNodeExecutable(baseDir, nodePath, MAX_PATH)) {
        int choice = MessageBoxW(
            NULL,
            L"Для работы высокоскоростного движка Aether Launcher требуется среда выполнения Node.js (LTS).\n\n"
            L"Нажмите «Да», чтобы открыть официальную страницу загрузки (nodejs.org), или установите Node.js вручную.",
            L"Aether Launcher — Требуется среда выполнения",
            MB_YESNO | MB_ICONINFORMATION
        );

        if (choice == IDYES) {
            ShellExecuteW(NULL, L"open", L"https://nodejs.org", NULL, NULL, SW_SHOWNORMAL);
        }
        CloseHandle(hMutex);
        return 1;
    }

    // Locate or self-extract dist/index.js
    WCHAR scriptPath[MAX_PATH];
    swprintf(scriptPath, MAX_PATH, L"%ls\\dist\\index.js", baseDir);

    if (!FileExists(scriptPath)) {
        swprintf(scriptPath, MAX_PATH, L"%ls\\index.js", baseDir);
    }

    if (!FileExists(scriptPath)) {
        // Self-extract embedded dist files into baseDir
        ExtractEmbeddedDist(baseDir);
        swprintf(scriptPath, MAX_PATH, L"%ls\\dist\\index.js", baseDir);
    }

    if (!FileExists(scriptPath)) {
        // Fallback: extract to %APPDATA%\AetherLauncher
        WCHAR appDataDir[MAX_PATH];
        ExpandEnvironmentStringsW(L"%APPDATA%\\AetherLauncher", appDataDir, MAX_PATH);
        ExtractEmbeddedDist(appDataDir);
        swprintf(scriptPath, MAX_PATH, L"%ls\\dist\\index.js", appDataDir);
        wcsncpy(baseDir, appDataDir, MAX_PATH);
    }

    if (!FileExists(scriptPath)) {
        MessageBoxW(
            NULL,
            L"Не удалось извлечь необходимые файлы лаунчера.\nПопробуйте запустить Aether от имени администратора или проверить права доступа к папке.",
            L"Aether Launcher — Ошибка",
            MB_OK | MB_ICONERROR
        );
        CloseHandle(hMutex);
        return 2;
    }

    // Launch server completely hidden without visible console window
    STARTUPINFOW siServer;
    PROCESS_INFORMATION piServer;
    ZeroMemory(&siServer, sizeof(siServer));
    siServer.cb = sizeof(siServer);
    siServer.dwFlags = STARTF_USESHOWWINDOW;
    siServer.wShowWindow = SW_HIDE;
    ZeroMemory(&piServer, sizeof(piServer));

    WCHAR cmdLine[2048];
    swprintf(cmdLine, 2048, L"\"%ls\" \"%ls\"", nodePath, scriptPath);

    BOOL success = CreateProcessW(
        NULL,
        cmdLine,
        NULL,
        NULL,
        FALSE,
        CREATE_NO_WINDOW | DETACHED_PROCESS,
        NULL,
        baseDir,
        &siServer,
        &piServer
    );

    if (!success) {
        WCHAR errBuf[256];
        swprintf(errBuf, 256, L"Не удалось запустить процесс лаунчера. Код ошибки: %lu", GetLastError());
        MessageBoxW(NULL, errBuf, L"Aether Launcher — Ошибка запуска", MB_OK | MB_ICONERROR);
        CloseHandle(hMutex);
        return 3;
    }

    // Wait 350ms for server to bind port
    Sleep(350);

    // Launch in Dedicated Desktop Application Window Mode
    WCHAR browserPath[MAX_PATH];
    WCHAR appDataDir[MAX_PATH];
    ExpandEnvironmentStringsW(L"%APPDATA%\\AetherLauncher", appDataDir, MAX_PATH);

    if (FindDesktopAppBrowser(browserPath, MAX_PATH)) {
        // Launch as a standalone desktop window without browser URL bar, tabs, or bookmarks!
        WCHAR appCmd[2048];
        swprintf(appCmd, 2048,
            L"\"%ls\" --app=\"http://localhost:3000\" --window-size=1150,730 --user-data-dir=\"%ls\\app_profile\" --app-id=\"AetherLauncher\"",
            browserPath, appDataDir
        );

        STARTUPINFOW siApp;
        PROCESS_INFORMATION piApp;
        ZeroMemory(&siApp, sizeof(siApp));
        siApp.cb = sizeof(siApp);
        ZeroMemory(&piApp, sizeof(piApp));

        if (CreateProcessW(NULL, appCmd, NULL, NULL, FALSE, 0, NULL, baseDir, &siApp, &piApp)) {
            // Desktop application window is running!
            // Wait for the desktop app window to close
            WaitForSingleObject(piApp.hProcess, INFINITE);

            // Once the desktop window is closed by the user, terminate server and exit cleanly!
            if (piServer.hProcess) {
                TerminateProcess(piServer.hProcess, 0);
                CloseHandle(piServer.hProcess);
                CloseHandle(piServer.hThread);
            }

            CloseHandle(piApp.hProcess);
            CloseHandle(piApp.hThread);
            CloseHandle(hMutex);
            return 0;
        }
    }

    // Fallback if no desktop app browser found: open default browser
    ShellExecuteW(NULL, L"open", L"http://localhost:3000", NULL, NULL, SW_SHOWNORMAL);

    // Keep server running
    CloseHandle(piServer.hThread);
    CloseHandle(piServer.hProcess);
    CloseHandle(hMutex);

    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    return wWinMain(hInstance, hPrevInstance, GetCommandLineW(), nCmdShow);
}
