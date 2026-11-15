#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <string>

static void EnsureDir(const std::wstring& path) {
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

static bool CreateShortcut(const std::wstring& targetPath, const std::wstring& shortcutPath, const std::wstring& workDir) {
    WCHAR cmd[2048];
    swprintf(cmd, 2048,
        L"powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "
        L"\"$ws = New-Object -ComObject WScript.Shell; $s = $ws.CreateShortcut('%ls'); "
        L"$s.TargetPath = '%ls'; $s.WorkingDirectory = '%ls'; $s.Save()\"",
        shortcutPath.c_str(), targetPath.c_str(), workDir.c_str()
    );

    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi;

    if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return true;
    }
    return false;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WCHAR currentExe[MAX_PATH];
    GetModuleFileNameW(NULL, currentExe, MAX_PATH);

    std::wstring srcDir = currentExe;
    size_t slash = srcDir.find_last_of(L"\\/");
    if (slash != std::wstring::npos) srcDir = srcDir.substr(0, slash);

    WCHAR appData[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appData);
    std::wstring destDir = std::wstring(appData) + L"\\AetherLauncher";

    std::wstring prompt = L"Добро пожаловать в установщик Aether Launcher v1.0.38 (Native C++)!\n\n"
                          L"Лаунчер будет установлен в папку:\n" + destDir + L"\n\n"
                          L"Продолжить установку?";

    int choice = MessageBoxW(NULL, prompt.c_str(), L"Aether Launcher Setup", MB_YESNO | MB_ICONQUESTION);
    if (choice != IDYES) return 0;

    EnsureDir(destDir);

    std::wstring srcAether = srcDir + L"\\Aether.exe";
    std::wstring dstAether = destDir + L"\\Aether.exe";

    if (GetFileAttributesW(srcAether.c_str()) != INVALID_FILE_ATTRIBUTES) {
        CopyFileW(srcAether.c_str(), dstAether.c_str(), FALSE);
    } else {
        CopyFileW(currentExe, dstAether.c_str(), FALSE);
    }

    // Create Desktop Shortcut
    WCHAR desktop[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desktop);
    std::wstring desktopLnk = std::wstring(desktop) + L"\\Aether Launcher.lnk";
    CreateShortcut(dstAether, desktopLnk, destDir);

    // Create Start Menu Shortcut
    WCHAR startMenu[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_PROGRAMS, NULL, 0, startMenu);
    std::wstring startLnk = std::wstring(startMenu) + L"\\Aether Launcher.lnk";
    CreateShortcut(dstAether, startLnk, destDir);

    int launch = MessageBoxW(
        NULL,
        L"Aether Launcher успешно установлен на ваш компьютер!\n"
        L"Ярлык создан на Рабочем столе.\n\n"
        L"Запустить Aether Launcher прямо сейчас?",
        L"Установка завершена",
        MB_YESNO | MB_ICONINFORMATION
    );

    if (launch == IDYES) {
        ShellExecuteW(NULL, L"open", dstAether.c_str(), NULL, destDir.c_str(), SW_SHOWNORMAL);
    }

    return 0;
}
