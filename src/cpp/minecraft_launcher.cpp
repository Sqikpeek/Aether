#include "minecraft_launcher.hpp"
#include <shlwapi.h>
#include <shlobj.h>
#include <stdio.h>
#include <algorithm>
#include <unordered_set>

static bool CheckFileExists(const std::wstring& path) {
    DWORD dwAttrib = GetFileAttributesW(path.c_str());
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

static void EnsureDirTree(const std::wstring& path) {
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

static void CollectJarsRecursively(const std::wstring& dir, std::vector<std::wstring>& outJars) {
    WIN32_FIND_DATAW fd;
    std::wstring searchPattern = dir + L"\\*";
    HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        std::wstring fullPath = dir + L"\\" + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            CollectJarsRecursively(fullPath, outJars);
        } else {
            size_t len = wcslen(fd.cFileName);
            if (len > 4 && _wcsicmp(fd.cFileName + len - 4, L".jar") == 0) {
                outJars.push_back(fullPath);
            }
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
}

struct VersionLibInfo {
    std::wstring path;
    std::wstring url;
};

static std::vector<VersionLibInfo> ParseLibrariesFromJson(const std::string& json) {
    std::vector<VersionLibInfo> result;
    size_t libPos = json.find("\"libraries\":");
    if (libPos == std::string::npos) return result;

    // 1. Search for artifact blocks
    size_t cur = libPos;
    while ((cur = json.find("\"artifact\":", cur)) != std::string::npos) {
        size_t nextLib = cur + 1200;
        std::string block = json.substr(cur, nextLib > json.size() ? json.size() - cur : 1200);

        size_t pathPos = block.find("\"path\":");
        size_t urlPos = block.find("\"url\":");
        if (pathPos != std::string::npos && urlPos != std::string::npos) {
            size_t q1 = block.find("\"", pathPos + 7);
            if (q1 != std::string::npos) {
                size_t q2 = block.find("\"", q1 + 1);
                if (q2 != std::string::npos) {
                    std::string pStr = block.substr(q1 + 1, q2 - q1 - 1);
                    size_t u1 = block.find("\"", urlPos + 6);
                    if (u1 != std::string::npos) {
                        size_t u2 = block.find("\"", u1 + 1);
                        if (u2 != std::string::npos) {
                            std::string uStr = block.substr(u1 + 1, u2 - u1 - 1);
                            for (char& c : pStr) {
                                if (c == '/') c = '\\';
                            }
                            VersionLibInfo v;
                            v.path = std::wstring(pStr.begin(), pStr.end());
                            v.url = std::wstring(uStr.begin(), uStr.end());
                            result.push_back(v);
                        }
                    }
                }
            }
        }
        cur += 11;
    }

    // 2. Search for Windows natives classifiers
    cur = libPos;
    while ((cur = json.find("\"natives-windows\":", cur)) != std::string::npos) {
        size_t pathPos = json.find("\"path\":", cur);
        size_t urlPos = json.find("\"url\":", cur);
        if (pathPos != std::string::npos && urlPos != std::string::npos && pathPos < cur + 500 && urlPos < cur + 500) {
            size_t q1 = json.find("\"", pathPos + 7);
            size_t q2 = (q1 != std::string::npos) ? json.find("\"", q1 + 1) : std::string::npos;
            size_t u1 = json.find("\"", urlPos + 6);
            size_t u2 = (u1 != std::string::npos) ? json.find("\"", u1 + 1) : std::string::npos;
            if (q2 != std::string::npos && u2 != std::string::npos) {
                std::string pStr = json.substr(q1 + 1, q2 - q1 - 1);
                std::string uStr = json.substr(u1 + 1, u2 - u1 - 1);
                for (char& c : pStr) {
                    if (c == '/') c = '\\';
                }
                VersionLibInfo v;
                v.path = std::wstring(pStr.begin(), pStr.end());
                v.url = std::wstring(uStr.begin(), uStr.end());
                result.push_back(v);
            }
        }
        cur += 18;
    }

    return result;
}

static std::vector<VersionLibInfo> GetStandardFallbackLibraries() {
    return {
        { L"net\\sf\\jopt-simple\\jopt-simple\\5.0.4\\jopt-simple-5.0.4.jar",
          L"https://libraries.minecraft.net/net/sf/jopt-simple/jopt-simple/5.0.4/jopt-simple-5.0.4.jar" },
        { L"com\\google\\code\\gson\\gson\\2.10.1\\gson-2.10.1.jar",
          L"https://libraries.minecraft.net/com/google/code/gson/gson/2.10.1/gson-2.10.1.jar" },
        { L"com\\google\\guava\\guava\\32.1.2-jre\\guava-32.1.2-jre.jar",
          L"https://libraries.minecraft.net/com/google/guava/guava/32.1.2-jre/guava-32.1.2-jre.jar" },
        { L"com\\mojang\\authlib\\6.0.55\\authlib-6.0.55.jar",
          L"https://libraries.minecraft.net/com/mojang/authlib/6.0.55/authlib-6.0.55.jar" },
        { L"com\\mojang\\brigadier\\1.2.9\\brigadier-1.2.9.jar",
          L"https://libraries.minecraft.net/com/mojang/brigadier/1.2.9/brigadier-1.2.9.jar" },
        { L"com\\mojang\\datafixerupper\\8.0.16\\datafixerupper-8.0.16.jar",
          L"https://libraries.minecraft.net/com/mojang/datafixerupper/8.0.16/datafixerupper-8.0.16.jar" },
        { L"it\\unimi\\dsi\\fastutil\\8.5.12\\fastutil-8.5.12.jar",
          L"https://libraries.minecraft.net/it/unimi/dsi/fastutil/8.5.12/fastutil-8.5.12.jar" },
        { L"commons-io\\commons-io\\2.15.1\\commons-io-2.15.1.jar",
          L"https://libraries.minecraft.net/commons-io/commons-io/2.15.1/commons-io-2.15.1.jar" },
        { L"commons-codec\\commons-codec\\1.16.0\\commons-codec-1.16.0.jar",
          L"https://libraries.minecraft.net/commons-codec/commons-codec/1.16.0/commons-codec-1.16.0.jar" },
        { L"org\\apache\\commons\\commons-lang3\\3.14.0\\commons-lang3-3.14.0.jar",
          L"https://libraries.minecraft.net/org/apache/commons/commons-lang3/3.14.0/commons-lang3-3.14.0.jar" },
        { L"org\\apache\\logging\\log4j\\log4j-api\\2.22.1\\log4j-api-2.22.1.jar",
          L"https://libraries.minecraft.net/org/apache/logging/log4j/log4j-api/2.22.1/log4j-api-2.22.1.jar" },
        { L"org\\apache\\logging\\log4j\\log4j-core\\2.22.1\\log4j-core-2.22.1.jar",
          L"https://libraries.minecraft.net/org/apache/logging/log4j/log4j-core/2.22.1/log4j-core-2.22.1.jar" },
        { L"org\\slf4j\\slf4j-api\\2.0.9\\slf4j-api-2.0.9.jar",
          L"https://libraries.minecraft.net/org/slf4j/slf4j-api/2.0.9/slf4j-api-2.0.9.jar" },
        { L"com\\github\\oshi\\oshi-core\\6.4.10\\oshi-core-6.4.10.jar",
          L"https://libraries.minecraft.net/com/github/oshi/oshi-core/6.4.10/oshi-core-6.4.10.jar" },
        { L"net\\java\\dev\\jna\\jna\\5.14.0\\jna-5.14.0.jar",
          L"https://libraries.minecraft.net/net/java/dev/jna/jna/5.14.0/jna-5.14.0.jar" },
        { L"net\\java\\dev\\jna\\jna-platform\\5.14.0\\jna-platform-5.14.0.jar",
          L"https://libraries.minecraft.net/net/java/dev/jna/jna-platform/5.14.0/jna-platform-5.14.0.jar" },
        { L"com\\ibm\\icu\\icu4j\\73.2\\icu4j-73.2.jar",
          L"https://libraries.minecraft.net/com/ibm/icu/icu4j/73.2/icu4j-73.2.jar" },
        { L"org\\lwjgl\\lwjgl\\3.3.3\\lwjgl-3.3.3.jar",
          L"https://libraries.minecraft.net/org/lwjgl/lwjgl/3.3.3/lwjgl-3.3.3.jar" },
        { L"org\\lwjgl\\lwjgl-jemalloc\\3.3.3\\lwjgl-jemalloc-3.3.3.jar",
          L"https://libraries.minecraft.net/org/lwjgl/lwjgl-jemalloc/3.3.3/lwjgl-jemalloc-3.3.3.jar" },
        { L"org\\lwjgl\\lwjgl-openal\\3.3.3\\lwjgl-openal-3.3.3.jar",
          L"https://libraries.minecraft.net/org/lwjgl/lwjgl-openal/3.3.3/lwjgl-openal-3.3.3.jar" },
        { L"org\\lwjgl\\lwjgl-opengl\\3.3.3\\lwjgl-opengl-3.3.3.jar",
          L"https://libraries.minecraft.net/org/lwjgl/lwjgl-opengl/3.3.3/lwjgl-opengl-3.3.3.jar" },
        { L"org\\lwjgl\\lwjgl-glfw\\3.3.3\\lwjgl-glfw-3.3.3.jar",
          L"https://libraries.minecraft.net/org/lwjgl/lwjgl-glfw/3.3.3/lwjgl-glfw-3.3.3.jar" },
        { L"org\\lwjgl\\lwjgl-stb\\3.3.3\\lwjgl-stb-3.3.3.jar",
          L"https://libraries.minecraft.net/org/lwjgl/lwjgl-stb/3.3.3/lwjgl-stb-3.3.3.jar" },
        { L"org\\lwjgl\\lwjgl-tinyfd\\3.3.3\\lwjgl-tinyfd-3.3.3.jar",
          L"https://libraries.minecraft.net/org/lwjgl/lwjgl-tinyfd/3.3.3/lwjgl-tinyfd-3.3.3.jar" }
    };
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
                std::string versionOutput = "";
                while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                    buffer[bytesRead] = 0;
                    versionOutput += buffer;
                }

                CloseHandle(hRead);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);

                JavaInfo info;
                info.path = cand;
                info.vendor = L"Java Runtime";
                info.is64Bit = (versionOutput.find("64-Bit") != std::string::npos);

                // Parse version (e.g., "17.0.8", "21.0.1", "1.8.0_381")
                size_t vpos = versionOutput.find("version \"");
                if (vpos != std::string::npos) {
                    vpos += 9;
                    int major = 0;
                    if (versionOutput.compare(vpos, 2, "1.") == 0) {
                        major = versionOutput[vpos + 2] - '0';
                    } else {
                        major = atoi(versionOutput.c_str() + vpos);
                    }
                    info.majorVersion = major;
                }

                if (versionOutput.find("Temurin") != std::string::npos) info.vendor = L"Eclipse Temurin";
                else if (versionOutput.find("HotSpot") != std::string::npos) info.vendor = L"OpenJDK HotSpot";
                else if (versionOutput.find("Corretto") != std::string::npos) info.vendor = L"Amazon Corretto";
                else if (versionOutput.find("Zulu") != std::string::npos) info.vendor = L"Azul Zulu";

                return info;
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
        EnsureDirTree(dirBuf);
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
    EnsureDirTree(versionDir);

    std::wstring libDir = storageDir + L"\\libraries";
    EnsureDirTree(libDir);

    WCHAR appData[MAX_PATH] = { 0 };
    std::wstring mojangLibsDir = L"";
    if (GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH) > 0) {
        mojangLibsDir = std::wstring(appData) + L"\\.minecraft\\libraries";
    }

    // Step 1: Ensure version.json is available
    if (!CheckFileExists(versionJson)) {
        if (progress) progress(L"Загрузка манифеста версии Minecraft...", 10);
        std::wstring manifestUrl = L"https://piston-meta.mojang.com/mc/game/version_manifest_v2.json";
        std::wstring manifestFile = storageDir + L"\\version_manifest_v2.json";
        DownloadHttpFile(manifestUrl, manifestFile, nullptr);

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
                            DownloadHttpFile(wideVUrl, versionJson, nullptr);
                        }
                    }
                }
            }
            fclose(fp);
        }
    }

    // Step 2: Read version.json and download client.jar if missing
    std::string versionJsonContent = "";
    FILE* vfp = _wfopen(versionJson.c_str(), L"rb");
    if (vfp) {
        fseek(vfp, 0, SEEK_END);
        long vsz = ftell(vfp);
        fseek(vfp, 0, SEEK_SET);
        if (vsz > 0 && vsz < 10000000) {
            std::vector<char> vbuf(vsz + 1, 0);
            fread(vbuf.data(), 1, vsz, vfp);
            versionJsonContent = vbuf.data();
        }
        fclose(vfp);
    }

    if (!CheckFileExists(versionJar)) {
        std::string clientJarUrl = "";
        if (!versionJsonContent.empty()) {
            char* clientBlock = strstr((char*)versionJsonContent.c_str(), "\"client\":");
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
        if (clientJarUrl.empty()) {
            clientJarUrl = "https://launcher.mojang.com/v1/objects/" + std::string(mcVersion.begin(), mcVersion.end()) + "/client.jar";
        }

        std::wstring wideClientUrl(clientJarUrl.begin(), clientJarUrl.end());
        if (progress) progress(L"Загрузка официального client.jar...", 25);
        DownloadHttpFile(wideClientUrl, versionJar, [&](int pct) {
            if (progress) {
                int overall = 25 + (pct * 30) / 100;
                WCHAR statusBuf[128];
                swprintf(statusBuf, 128, L"Загрузка client.jar (%d%%)...", pct);
                progress(statusBuf, overall);
            }
        });
    }

    // Step 3: Parse and ensure ALL Minecraft Libraries (e.g. joptsimple, guava, gson, lwjgl)
    std::vector<VersionLibInfo> libsToEnsure;
    if (!versionJsonContent.empty()) {
        libsToEnsure = ParseLibrariesFromJson(versionJsonContent);
    }
    if (libsToEnsure.empty()) {
        libsToEnsure = GetStandardFallbackLibraries();
    }

    if (progress) progress(L"Проверка и синхронизация библиотек...", 60);

    int totalLibs = (int)libsToEnsure.size();
    for (int i = 0; i < totalLibs; ++i) {
        const auto& lib = libsToEnsure[i];
        std::wstring targetPath = libDir + L"\\" + lib.path;

        if (!CheckFileExists(targetPath)) {
            // Check if available in standard %APPDATA%\.minecraft\libraries
            bool foundInAppData = false;
            if (!mojangLibsDir.empty()) {
                std::wstring appDataLibPath = mojangLibsDir + L"\\" + lib.path;
                if (CheckFileExists(appDataLibPath)) {
                    // Copy existing library locally
                    EnsureDirTree(targetPath);
                    CopyFileW(appDataLibPath.c_str(), targetPath.c_str(), FALSE);
                    foundInAppData = true;
                }
            }

            // If not found in local AppData, download from Mojang CDN
            if (!foundInAppData && !lib.url.empty()) {
                DownloadHttpFile(lib.url, targetPath, nullptr);
            }
        }

        if (progress && (i % 5 == 0 || i == totalLibs - 1)) {
            int pct = 60 + (i * 35) / totalLibs;
            WCHAR lBuf[128];
            swprintf(lBuf, 128, L"Синхронизация библиотек (%d/%d)...", i + 1, totalLibs);
            progress(lBuf, pct);
        }
    }

    if (progress) progress(L"Файлы игры успешно подготовлены!", 100);
    return true;
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

    // 2. Prepare Game Files (client.jar and Mojang libraries)
    if (callbacks.onProgress) callbacks.onProgress(L"Подготовка файлов игры и библиотек...", 15);
    bool filesOk = PrepareMinecraftFiles(storageDir, mcVersion, callbacks.onProgress);
    if (!filesOk) {
        if (callbacks.onLog) callbacks.onLog(L"[Aether Launcher] Ошибка: Не удалось подготовить файлы для версии " + mcVersion, true);
        if (callbacks.onProgress) callbacks.onProgress(L"Ошибка загрузки файлов", 0);
        return false;
    }

    // 3. Assemble directories and paths
    std::wstring instanceDir = storageDir + L"\\instances\\" + mcVersion;
    std::wstring assetsDir = storageDir + L"\\assets";
    std::wstring nativesDir = storageDir + L"\\versions\\" + mcVersion + L"\\natives";
    std::wstring clientJar = storageDir + L"\\versions\\" + mcVersion + L"\\" + mcVersion + L".jar";

    EnsureDirTree(storageDir + L"\\instances");
    EnsureDirTree(instanceDir);
    EnsureDirTree(assetsDir);
    EnsureDirTree(nativesDir);

    // 4. Build Complete Classpath Recursively (Resolving joptsimple, guava, lwjgl, etc.)
    std::vector<std::wstring> allJars;

    // Scan storageDir/libraries recursively
    std::wstring libDir = storageDir + L"\\libraries";
    CollectJarsRecursively(libDir, allJars);

    // Also scan %APPDATA%\.minecraft\libraries recursively
    WCHAR appData[MAX_PATH] = { 0 };
    if (GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH) > 0) {
        std::wstring mojangLibs = std::wstring(appData) + L"\\.minecraft\\libraries";
        if (PathFileExistsW(mojangLibs.c_str())) {
            CollectJarsRecursively(mojangLibs, allJars);
        }
    }

    // Client jar first
    std::wstring classpath = L"\"" + clientJar + L"\"";
    std::unordered_set<std::wstring> seenJarNames;

    size_t cSlash = clientJar.find_last_of(L"\\/");
    if (cSlash != std::wstring::npos) {
        std::wstring cName = clientJar.substr(cSlash + 1);
        std::transform(cName.begin(), cName.end(), cName.begin(), ::towlower);
        seenJarNames.insert(cName);
    }

    size_t addedCount = 0;
    for (const auto& jarPath : allJars) {
        size_t lastSlash = jarPath.find_last_of(L"\\/");
        std::wstring fname = (lastSlash == std::wstring::npos) ? jarPath : jarPath.substr(lastSlash + 1);
        std::wstring lowerName = fname;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::towlower);

        if (seenJarNames.find(lowerName) == seenJarNames.end()) {
            seenJarNames.insert(lowerName);
            classpath += L";\"" + jarPath + L"\"";
            addedCount++;
        }
    }

    if (callbacks.onLog) {
        WCHAR cpMsg[256];
        swprintf(cpMsg, 256, L"[Aether Launcher] Собрано библиотек в classpath: %zu jar-файлов", addedCount);
        callbacks.onLog(cpMsg, false);
    }

    // Determine Main Class
    std::wstring mainClass = L"net.minecraft.client.main.Main";
    std::wstring versionJsonPath = storageDir + L"\\versions\\" + mcVersion + L"\\" + mcVersion + L".json";
    FILE* vfp = _wfopen(versionJsonPath.c_str(), L"rb");
    if (vfp) {
        fseek(vfp, 0, SEEK_END);
        long vsz = ftell(vfp);
        fseek(vfp, 0, SEEK_SET);
        if (vsz > 0 && vsz < 5000000) {
            std::vector<char> vbuf(vsz + 1, 0);
            fread(vbuf.data(), 1, vsz, vfp);
            std::string content = vbuf.data();
            size_t mcPos = content.find("\"mainClass\": \"");
            if (mcPos != std::string::npos) {
                mcPos += 14;
                size_t mcEnd = content.find("\"", mcPos);
                if (mcEnd != std::string::npos) {
                    std::string mcs = content.substr(mcPos, mcEnd - mcPos);
                    mainClass = std::wstring(mcs.begin(), mcs.end());
                }
            }
        }
        fclose(vfp);
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
        mainClass + L" " +
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
        callbacks.onLog(L"[Aether Launcher] JVM Аргументы: " + (flagsToUse.empty() ? L"(по умолчанию)" : flagsToUse), false);
        callbacks.onLog(L"[Aether Launcher] Главный класс клиента: " + mainClass, false);
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
    if (logText.find(L"NoClassDefFoundError") != std::wstring::npos || logText.find(L"ClassNotFoundException") != std::wstring::npos) {
        return L"Отсутствует библиотека classpath (NoClassDefFoundError). Лаунчер Aether автоматически скачивает и собирает все необходимые библиотеки из официального репозитория Mojang и локального профиля.";
    }
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
