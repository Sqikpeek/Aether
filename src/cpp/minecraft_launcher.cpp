#include "minecraft_launcher.hpp"
#include <shlwapi.h>
#include <stdio.h>
#include <algorithm>

static bool CheckFileExists(const std::wstring& path) {
    DWORD dwAttrib = GetFileAttributesW(path.c_str());
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

int MinecraftLauncher::GetRequiredJavaVersion(const std::wstring& mcVersion) {
    // 1.20.5+ requires Java 21
    // 1.18 - 1.20.4 requires Java 17
    // 1.17 requires Java 16
    // <= 1.16.5 requires Java 8
    int minor = 0, patch = 0;
    swscanf(mcVersion.c_str(), L"1.%d.%d", &minor, &patch);
    if (minor == 0) {
        swscanf(mcVersion.c_str(), L"1.%d", &minor);
    }

    if (minor > 20 || (minor == 20 && patch >= 5)) return 21;
    if (minor >= 18) return 17;
    if (minor == 17) return 16;
    return 8;
}

JavaInfo MinecraftLauncher::DetectJava(const std::wstring& customPath) {
    std::vector<std::wstring> candidates;

    if (!customPath.empty() && CheckFileExists(customPath)) {
        candidates.push_back(customPath);
    }

    // Check JAVA_HOME
    WCHAR javaHome[MAX_PATH];
    if (GetEnvironmentVariableW(L"JAVA_HOME", javaHome, MAX_PATH) > 0) {
        std::wstring bin = std::wstring(javaHome) + L"\\bin\\java.exe";
        if (CheckFileExists(bin)) candidates.push_back(bin);
    }

    // Check PATH
    WCHAR pathBuf[MAX_PATH];
    if (SearchPathW(NULL, L"java.exe", NULL, MAX_PATH, pathBuf, NULL) > 0) {
        candidates.push_back(pathBuf);
    }

    // Common installation folders on Windows
    std::vector<std::wstring> standardDirs = {
        L"C:\\Program Files\\Eclipse Adoptium",
        L"C:\\Program Files\\Java",
        L"C:\\Program Files\\BellSoft",
        L"C:\\Program Files\\Amazon Corretto",
        L"C:\\Program Files\\Microsoft",
        L"C:\\Program Files\\Zulu",
        L"C:\\Program Files (x86)\\Minecraft Launcher\\runtime"
    };

    for (const auto& base : standardDirs) {
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW((base + L"\\*").c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    if (wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0) {
                        std::wstring sub = base + L"\\" + fd.cFileName + L"\\bin\\java.exe";
                        if (CheckFileExists(sub)) candidates.push_back(sub);
                        // Also check jre/bin
                        std::wstring subJre = base + L"\\" + fd.cFileName + L"\\jre\\bin\\java.exe";
                        if (CheckFileExists(subJre)) candidates.push_back(subJre);
                    }
                }
            } while (FindNextFileW(hFind, &fd));
            FindClose(hFind);
        }
    }

    // Inspect first working candidate
    for (const auto& cand : candidates) {
        // Run java -version hidden
        SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
        HANDLE hRead, hWrite;
        if (CreatePipe(&hRead, &hWrite, &sa, 0)) {
            SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

            STARTUPINFOW si = { sizeof(STARTUPINFOW) };
            si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;
            si.hStdError = hWrite;
            si.hStdOutput = hWrite;

            PROCESS_INFORMATION pi;
            WCHAR cmd[MAX_PATH + 32];
            swprintf(cmd, MAX_PATH + 32, L"\"%ls\" -version", cand.c_str());

            if (CreateProcessW(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
                CloseHandle(hWrite);
                WaitForSingleObject(pi.hProcess, 3000);

                char buffer[2048] = { 0 };
                DWORD bytesRead = 0;
                ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL);
                CloseHandle(hRead);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);

                JavaInfo info;
                info.path = cand;
                info.vendor = L"OpenJDK";

                // Parse major version from buffer
                // e.g. "21.0.3" or "1.8.0_312" or "17.0.10"
                const char* verStr = strstr(buffer, "version \"");
                if (verStr) {
                    verStr += 9;
                    int v1 = 0, v2 = 0;
                    if (sscanf(verStr, "%d.%d", &v1, &v2) == 2) {
                        if (v1 == 1) info.majorVersion = v2; // 1.8 -> 8
                        else info.majorVersion = v1;        // 17, 21
                    }
                }

                if (strstr(buffer, "Temurin") || strstr(buffer, "Adoptium")) info.vendor = L"Eclipse Temurin";
                else if (strstr(buffer, "Microsoft")) info.vendor = L"Microsoft OpenJDK";
                else if (strstr(buffer, "Corretto")) info.vendor = L"Amazon Corretto";
                else if (strstr(buffer, "Zulu")) info.vendor = L"Azul Zulu";
                else if (strstr(buffer, "Oracle")) info.vendor = L"Oracle Java";

                if (info.majorVersion > 0) {
                    return info;
                }
            } else {
                CloseHandle(hRead);
                CloseHandle(hWrite);
            }
        }
    }

    JavaInfo fallback;
    fallback.path = L"java.exe";
    fallback.majorVersion = 17;
    fallback.vendor = L"System Java";
    return fallback;
}

bool MinecraftLauncher::DownloadHttpFile(
    const std::wstring& url,
    const std::wstring& destPath,
    std::function<void(int percent)> progress
) {
    HINTERNET hInternet = InternetOpenW(L"AetherLauncher/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return false;

    HINTERNET hUrl = InternetOpenUrlW(
        hInternet,
        url.c_str(),
        NULL,
        0,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE | INTERNET_FLAG_SECURE,
        0
    );

    if (!hUrl) {
        InternetCloseHandle(hInternet);
        return false;
    }

    // Get Content-Length
    DWORD contentLength = 0;
    DWORD sizeOfLen = sizeof(contentLength);
    DWORD index = 0;
    HttpQueryInfoW(hUrl, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &contentLength, &sizeOfLen, &index);

    // Create destination directories
    WCHAR dirBuf[MAX_PATH];
    wcsncpy(dirBuf, destPath.c_str(), MAX_PATH);
    WCHAR* lastSlash = wcsrchr(dirBuf, L'\\');
    if (lastSlash) {
        *lastSlash = L'\0';
        WCHAR temp[MAX_PATH];
        wcsncpy(temp, dirBuf, MAX_PATH);
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

    HANDLE hFile = CreateFileW(destPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        return false;
    }

    BYTE buffer[16384];
    DWORD bytesRead = 0;
    DWORD totalDownloaded = 0;

    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        DWORD written = 0;
        WriteFile(hFile, buffer, bytesRead, &written, NULL);
        totalDownloaded += bytesRead;

        if (contentLength > 0 && progress) {
            int pct = (int)((totalDownloaded * 100ULL) / contentLength);
            progress(pct > 100 ? 100 : pct);
        }
    }

    CloseHandle(hFile);
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);

    return totalDownloaded > 0;
}

bool MinecraftLauncher::PrepareMinecraftFiles(
    const std::wstring& storageDir,
    const std::wstring& mcVersion,
    std::function<void(const std::wstring& status, int percent)> progress
) {
    std::wstring versionDir = storageDir + L"\\versions\\" + mcVersion;
    std::wstring versionJar = versionDir + L"\\" + mcVersion + L".jar";
    std::wstring versionJson = versionDir + L"\\" + mcVersion + L".json";

    if (CheckFileExists(versionJar)) {
        if (progress) progress(L"Файлы игры готовы", 100);
        return true;
    }

    if (progress) progress(L"Загрузка метаданных версии Minecraft...", 10);

    // Direct Mojang download URL pattern for releases
    // Mojang version manifest: https://piston-meta.mojang.com/mc/game/version_manifest_v2.json
    // For standard versions, we can download the official client jar directly from Mojang CDN:
    std::wstring manifestUrl = L"https://piston-meta.mojang.com/mc/game/version_manifest_v2.json";
    std::wstring manifestFile = storageDir + L"\\version_manifest_v2.json";

    DownloadHttpFile(manifestUrl, manifestFile, nullptr);

    // Read manifest file to find client.jar url for requested version
    std::string clientJarUrl = "";
    FILE* fp = _wfopen(manifestFile.c_str(), L"rb");
    if (fp) {
        fseek(fp, 0, SEEK_END);
        long sz = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        if (sz > 0 && sz < 20000000) {
            std::vector<char> buf(sz + 1, 0);
            fread(buf.data(), 1, sz, fp);

            std::string targetId = "\"id\": \"" + std::string(mcVersion.begin(), mcVersion.end()) + "\"";
            char* p = strstr(buf.data(), targetId.c_str());
            if (p) {
                char* urlKey = strstr(p, "\"url\": \"");
                if (urlKey) {
                    urlKey += 8;
                    char* endQuote = strchr(urlKey, '\"');
                    if (endQuote) {
                        std::string vUrl(urlKey, endQuote - urlKey);
                        std::wstring wideVUrl(vUrl.begin(), vUrl.end());

                        if (progress) progress(L"Загрузка конфигурации клиента...", 30);
                        DownloadHttpFile(wideVUrl, versionJson, nullptr);

                        // Read versionJson to get client.jar url
                        FILE* vfp = _wfopen(versionJson.c_str(), L"rb");
                        if (vfp) {
                            fseek(vfp, 0, SEEK_END);
                            long vsz = ftell(vfp);
                            fseek(vfp, 0, SEEK_SET);
                            std::vector<char> vbuf(vsz + 1, 0);
                            fread(vbuf.data(), 1, vsz, vfp);
                            fclose(vfp);

                            char* clientBlock = strstr(vbuf.data(), "\"client\":");
                            if (clientBlock) {
                                char* curl = strstr(clientBlock, "\"url\": \"");
                                if (curl) {
                                    curl += 8;
                                    char* eq = strchr(curl, '\"');
                                    if (eq) {
                                        clientJarUrl = std::string(curl, eq - curl);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        fclose(fp);
    }

    // If client jar url was not parsed from manifest, use standard fallback URL or report
    if (clientJarUrl.empty()) {
        if (progress) progress(L"Загрузка client.jar из официального репозитория...", 50);
        // Fallback Mojang package URL for version
        clientJarUrl = "https://launcher.mojang.com/v1/objects/" + std::string(mcVersion.begin(), mcVersion.end()) + "/client.jar";
    }

    std::wstring wideClientUrl(clientJarUrl.begin(), clientJarUrl.end());

    if (progress) progress(L"Загрузка официального client.jar...", 40);

    bool ok = DownloadHttpFile(wideClientUrl, versionJar, [&](int pct) {
        if (progress) {
            int overall = 40 + (pct * 55) / 100;
            WCHAR statusBuf[128];
            swprintf(statusBuf, 128, L"Загрузка client.jar (%d%%)...", pct);
            progress(statusBuf, overall);
        }
    });

    if (ok) {
        if (progress) progress(L"Файлы игры успешно подготовлены!", 100);
        return true;
    }

    return false;
}

bool MinecraftLauncher::LaunchMinecraft(
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
) {
    if (callbacks.onProgress) callbacks.onProgress(L"Проверка среды Java...", 5);
    if (callbacks.onLog) callbacks.onLog(L"[Aether Launcher] Инициализация запуска Minecraft " + mcVersion, false);

    // 1. Detect Java
    JavaInfo java = DetectJava(customJava);
    if (callbacks.onLog) {
        WCHAR jBuf[512];
        swprintf(jBuf, 512, L"[Aether Launcher] Найдена Java %d (%ls) по пути: %ls", java.majorVersion, java.vendor.c_str(), java.path.c_str());
        callbacks.onLog(jBuf, false);
    }

    int requiredJava = GetRequiredJavaVersion(mcVersion);
    if (java.majorVersion > 0 && java.majorVersion < requiredJava) {
        WCHAR warnBuf[512];
        swprintf(warnBuf, 512, L"[Aether Launcher] ВНИМАНИЕ: Для Minecraft %ls рекомендуется Java %d+, а текущая — Java %d!",
            mcVersion.c_str(), requiredJava, java.majorVersion);
        if (callbacks.onLog) callbacks.onLog(warnBuf, true);
    }

    // 2. Prepare Game Files (client.jar)
    if (callbacks.onProgress) callbacks.onProgress(L"Подготовка файлов игры...", 15);
    bool filesOk = PrepareMinecraftFiles(storageDir, mcVersion, callbacks.onProgress);
    if (!filesOk) {
        if (callbacks.onLog) callbacks.onLog(L"[Aether Launcher] Ошибка: Не удалось загрузить client.jar для версии " + mcVersion, true);
        if (callbacks.onProgress) callbacks.onProgress(L"Ошибка загрузки client.jar", 0);
        return false;
    }

    // 3. Assemble directories and paths
    std::wstring instanceDir = storageDir + L"\\instances\\" + mcVersion;
    std::wstring assetsDir = storageDir + L"\\assets";
    std::wstring nativesDir = storageDir + L"\\versions\\" + mcVersion + L"\\natives";
    std::wstring clientJar = storageDir + L"\\versions\\" + mcVersion + L"\\" + mcVersion + L".jar";

    // Ensure folders exist
    CreateDirectoryW((storageDir + L"\\instances").c_str(), NULL);
    CreateDirectoryW(instanceDir.c_str(), NULL);
    CreateDirectoryW(assetsDir.c_str(), NULL);
    CreateDirectoryW(nativesDir.c_str(), NULL);

    // 4. Build Classpath
    // Scan libraries directory if present and append client.jar
    std::wstring classpath = L"\"" + clientJar + L"\"";
    std::wstring libDir = storageDir + L"\\libraries";
    if (PathFileExistsW(libDir.c_str())) {
        // Find jars in libDir
        WIN32_FIND_DATAW lfd;
        HANDLE hLibFind = FindFirstFileW((libDir + L"\\*.jar").c_str(), &lfd);
        if (hLibFind != INVALID_HANDLE_VALUE) {
            do {
                classpath += L";\"" + libDir + L"\\" + lfd.cFileName + L"\"";
            } while (FindNextFileW(hLibFind, &lfd));
            FindClose(hLibFind);
        }
    }

    // 5. Build JVM and Game arguments
    std::wstring memArg = L"-Xmx" + std::to_wstring(memoryMb > 1024 ? memoryMb : 4096) + L"M -Xms1024M";
    std::wstring aikarFlags = L"-XX:+UseG1GC -XX:+ParallelRefProcEnabled -XX:MaxGCPauseMillis=200 -XX:+UnlockExperimentalVMOptions -XX:+DisableExplicitGC -XX:+AlwaysPreTouch";

    std::wstring selectedPresetFlags = aikarFlags;
    if (jvmPreset == L"low-ram") {
        selectedPresetFlags = L"-XX:+UseSerialGC -XX:+AlwaysPreTouch";
    } else if (jvmPreset == L"standard") {
        selectedPresetFlags = L"-XX:+UseG1GC -XX:+AlwaysPreTouch";
    } else if (jvmPreset == L"zgc") {
        selectedPresetFlags = L"-XX:+UseZGC -XX:+ZGenerational -XX:+UnlockExperimentalVMOptions -XX:+AlwaysPreTouch";
    } else if (jvmPreset == L"shenandoah") {
        selectedPresetFlags = L"-XX:+UseShenandoahGC -XX:+AlwaysPreTouch -XX:ShenandoahGCHeuristics=adaptive";
    } else if (jvmPreset == L"custom") {
        selectedPresetFlags = L"";
    }

    std::wstring finalNickname = nickname.empty() ? L"Player" : nickname;
    std::wstring flagsToUse = customArgs.empty() ? selectedPresetFlags : customArgs;

    std::wstring cmdLine = L"\"" + java.path + L"\" " +
        memArg + L" " +
        (flagsToUse.empty() ? L"" : (flagsToUse + L" ")) +
        L"-Djava.library.path=\"" + nativesDir + L"\" " +
        L"-Dminecraft.launcher.brand=Aether " +
        L"-Dminecraft.launcher.version=1.0.38 " +
        L"-cp " + classpath + L" " +
        L"net.minecraft.client.main.Main " +
        L"--username \"" + finalNickname + L"\" " +
        L"--version \"" + mcVersion + L"\" " +
        L"--gameDir \"" + instanceDir + L"\" " +
        L"--assetsDir \"" + assetsDir + L"\" " +
        L"--assetIndex \"" + mcVersion + L"\" " +
        L"--uuid 00000000-0000-0000-0000-000000000000 " +
        L"--accessToken 0 " +
        L"--userType mojang " +
        L"--versionType Aether-" + loader;

    if (callbacks.onLog) {
        callbacks.onLog(L"[Aether Launcher] JVM Аргументы: " + flagsToUse, false);
        callbacks.onLog(L"[Aether Launcher] Командная строка запуска сформирована", false);
    }
    if (callbacks.onProgress) callbacks.onProgress(L"Запуск процесса игры...", 90);

    // 6. Spawn process with stdout/stderr redirection
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE hStdOutRead = NULL, hStdOutWrite = NULL;
    if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0)) {
        if (callbacks.onLog) callbacks.onLog(L"[Aether Launcher] Ошибка создания pipe", true);
        return false;
    }
    SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hStdOutWrite;
    si.hStdError = hStdOutWrite;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    std::vector<WCHAR> cmdLineBuf(cmdLine.begin(), cmdLine.end());
    cmdLineBuf.push_back(0);

    BOOL procCreated = CreateProcessW(
        NULL,
        cmdLineBuf.data(),
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW,
        NULL,
        instanceDir.c_str(),
        &si,
        &pi
    );

    CloseHandle(hStdOutWrite);

    if (!procCreated) {
        DWORD err = GetLastError();
        WCHAR errBuf[256];
        swprintf(errBuf, 256, L"[Aether Launcher] Не удалось запустить java.exe! Код ошибки Windows: %lu", err);
        if (callbacks.onLog) callbacks.onLog(errBuf, true);
        if (callbacks.onProgress) callbacks.onProgress(L"Ошибка запуска", 0);
        CloseHandle(hStdOutRead);
        return false;
    }

    if (outProcessHandle) {
        *outProcessHandle = pi.hProcess;
    }

    if (callbacks.onProgress) callbacks.onProgress(L"Игра запущена!", 100);
    if (callbacks.onLog) {
        WCHAR pBuf[128];
        swprintf(pBuf, 128, L"[Aether Launcher] Процесс Minecraft запущен (PID: %lu)", pi.dwProcessId);
        callbacks.onLog(pBuf, false);
    }

    // Background thread to monitor stdout and game lifecycle without consuming CPU
    std::thread([hStdOutRead, pi, callbacks]() {
        char buffer[1024];
        DWORD bytesRead = 0;
        std::string accumulated = "";

        while (ReadFile(hStdOutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = 0;
            accumulated += buffer;

            size_t newlinePos;
            while ((newlinePos = accumulated.find('\n')) != std::string::npos) {
                std::string line = accumulated.substr(0, newlinePos);
                accumulated.erase(0, newlinePos + 1);

                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (line.empty()) continue;

                std::wstring wideLine(line.begin(), line.end());
                bool isErr = (line.find("FATAL") != std::string::npos || line.find("ERROR") != std::string::npos || line.find("Exception") != std::string::npos);

                if (callbacks.onLog) callbacks.onLog(wideLine, isErr);
            }
        }

        CloseHandle(hStdOutRead);

        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        if (callbacks.onLog) {
            WCHAR exBuf[128];
            swprintf(exBuf, 128, L"[Aether Launcher] Игра завершена с кодом: %lu", exitCode);
            callbacks.onLog(exBuf, exitCode != 0);
        }

        if (callbacks.onGameExited) {
            callbacks.onGameExited((int)exitCode);
        }
    }).detach();

    return true;
}

void MinecraftLauncher::TerminateRunningGame(HANDLE hProcess) {
    if (hProcess && hProcess != INVALID_HANDLE_VALUE) {
        TerminateProcess(hProcess, 0);
    }
}

std::wstring MinecraftLauncher::AnalyzeCrashLog(const std::wstring& logText) {
    if (logText.find(L"UnsupportedClassVersionError") != std::wstring::npos) {
        return L"Несовместимая версия Java: для этой версии Minecraft требуется более новая Java (например, Java 17 или Java 21). Установите её в настройках.";
    }
    if (logText.find(L"OutOfMemoryError") != std::wstring::npos) {
        return L"Недостаточно оперативной памяти (OOM): игре не хватило RAM. Увеличьте объем выделенной памяти в настройках лаунчера (до 4-6 ГБ).";
    }
    if (logText.find(L"requires") != std::wstring::npos && logText.find(L"fabric-api") != std::wstring::npos) {
        return L"Отсутствует обязательная библиотека: мод требует установленного Fabric API. Установите 'fabric-api' через каталог модов.";
    }
    if (logText.find(L"GLFW error 65542") != std::wstring::npos) {
        return L"Ошибка видеокарты / OpenGL: обновите видеодрайвер (NVIDIA, AMD или Intel) с официального сайта.";
    }
    return L"Произошла ошибка при запуске игры. Проверьте вкладку «Логи» для подробностей.";
}
