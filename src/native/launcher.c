#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdbool.h>

static bool FileExists(const WCHAR* path) {
    DWORD dwAttrib = GetFileAttributesW(path);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
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

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
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
        return 1;
    }

    // Verify dist/index.js exists
    WCHAR scriptPath[MAX_PATH];
    swprintf(scriptPath, MAX_PATH, L"%ls\\dist\\index.js", baseDir);
    if (!FileExists(scriptPath)) {
        // Check if index.js is directly in baseDir
        swprintf(scriptPath, MAX_PATH, L"%ls\\index.js", baseDir);
        if (!FileExists(scriptPath)) {
            MessageBoxW(
                NULL,
                L"Файлы лаунчера повреждены или не найдены (dist\\index.js отсутствует).\n"
                L"Пожалуйста, запустите установщик Aether Installer заново.",
                L"Aether Launcher — Ошибка",
                MB_OK | MB_ICONERROR
            );
            return 2;
        }
    }

    // Launch server completely hidden without visible console window
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    ZeroMemory(&pi, sizeof(pi));

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
        &si,
        &pi
    );

    if (!success) {
        WCHAR errBuf[256];
        swprintf(errBuf, 256, L"Не удалось запустить процесс лаунчера. Код ошибки: %lu", GetLastError());
        MessageBoxW(NULL, errBuf, L"Aether Launcher — Ошибка запуска", MB_OK | MB_ICONERROR);
        return 3;
    }

    // Close process handles as we run detached
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    // Wait 400ms for server to bind port
    Sleep(400);

    // Open launcher interface in default browser or webview
    ShellExecuteW(NULL, L"open", L"http://localhost:3000", NULL, NULL, SW_SHOWNORMAL);

    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    return wWinMain(hInstance, hPrevInstance, GetCommandLineW(), nCmdShow);
}
