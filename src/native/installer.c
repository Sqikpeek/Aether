#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdbool.h>

static void CopyDirRecursive(const WCHAR* src, const WCHAR* dest) {
    CreateDirectoryW(dest, NULL);
    WCHAR searchPath[MAX_PATH];
    swprintf(searchPath, MAX_PATH, L"%ls\\*.*", src);

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(searchPath, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;

        WCHAR srcSub[MAX_PATH];
        WCHAR destSub[MAX_PATH];
        swprintf(srcSub, MAX_PATH, L"%ls\\%ls", src, fd.cFileName);
        swprintf(destSub, MAX_PATH, L"%ls\\%ls", dest, fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            CopyDirRecursive(srcSub, destSub);
        } else {
            CopyFileW(srcSub, destSub, FALSE);
        }
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
}

static bool CreateShortcut(const WCHAR* targetPath, const WCHAR* shortcutPath, const WCHAR* workDir) {
    // Call powershell to create shortcut safely without COM threading issues
    WCHAR cmd[2048];
    swprintf(cmd, 2048,
        L"powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "
        L"\"$ws = New-Object -ComObject WScript.Shell; $s = $ws.CreateShortcut('%ls'); "
        L"$s.TargetPath = '%ls'; $s.WorkingDirectory = '%ls'; $s.Save()\"",
        shortcutPath, targetPath, workDir
    );

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    ZeroMemory(&pi, sizeof(pi));

    if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return true;
    }
    return false;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    WCHAR currentExe[MAX_PATH];
    GetModuleFileNameW(NULL, currentExe, MAX_PATH);

    WCHAR srcDir[MAX_PATH];
    wcsncpy(srcDir, currentExe, MAX_PATH);
    WCHAR* slash = wcsrchr(srcDir, L'\\');
    if (slash) *slash = L'\0';

    // Target default install folder: %APPDATA%\AetherLauncher
    WCHAR destDir[MAX_PATH];
    ExpandEnvironmentStringsW(L"%APPDATA%\\AetherLauncher", destDir, MAX_PATH);

    WCHAR promptMsg[1024];
    swprintf(promptMsg, 1024,
        L"Добро пожаловать в программу установки Aether Launcher v1.0.38!\n\n"
        L"Лаунчер будет установлен в папку:\n%ls\n\n"
        L"Продолжить установку?",
        destDir
    );

    int choice = MessageBoxW(
        NULL,
        promptMsg,
        L"Aether Launcher Setup",
        MB_YESNO | MB_ICONQUESTION
    );

    if (choice != IDYES) {
        return 0;
    }

    // Create target directory and copy files
    CreateDirectoryW(destDir, NULL);
    CopyDirRecursive(srcDir, destDir);

    // Target executable
    WCHAR targetExe[MAX_PATH];
    swprintf(targetExe, MAX_PATH, L"%ls\\Aether.exe", destDir);

    // Create Desktop Shortcut
    WCHAR desktopLnk[MAX_PATH];
    ExpandEnvironmentStringsW(L"%USERPROFILE%\\Desktop\\Aether Launcher.lnk", desktopLnk, MAX_PATH);
    CreateShortcut(targetExe, desktopLnk, destDir);

    // Create Start Menu Shortcut
    WCHAR startMenuLnk[MAX_PATH];
    ExpandEnvironmentStringsW(L"%APPDATA%\\Microsoft\\Windows\\Start Menu\\Programs\\Aether Launcher.lnk", startMenuLnk, MAX_PATH);
    CreateShortcut(targetExe, startMenuLnk, destDir);

    int launchNow = MessageBoxW(
        NULL,
        L"Aether Launcher успешно установлен на ваш компьютер!\n"
        L"Ярлык создан на Рабочем столе.\n\n"
        L"Запустить Aether Launcher прямо сейчас?",
        L"Установка завершена",
        MB_YESNO | MB_ICONINFORMATION
    );

    if (launchNow == IDYES) {
        ShellExecuteW(NULL, L"open", targetExe, NULL, destDir, SW_SHOWNORMAL);
    }

    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    return wWinMain(hInstance, hPrevInstance, GetCommandLineW(), nCmdShow);
}
