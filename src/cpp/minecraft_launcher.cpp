#include "minecraft_launcher.hpp"
#include <shlwapi.h>
#include <shlobj.h>
#include <stdio.h>
#include <algorithm>
#include <unordered_set>

std::wstring MinecraftLauncher::CleanPath(const std::wstring& path) {
    if (path.empty()) return path;
    std::wstring result;
    bool lastWasSlash = false;
    for (size_t i = 0; i < path.size(); ++i) {
        wchar_t c = path[i];
        if (c == L'/') c = L'\\';
        if (c == L'\\') {
            if (i == 1 && path[0] == L'\\') {
                // UNC path root: \\server\share
                result.push_back(c);
            } else if (!lastWasSlash) {
                result.push_back(c);
                lastWasSlash = true;
            }
        } else {
            result.push_back(c);
            lastWasSlash = false;
        }
    }
    while (result.size() > 3 && result.back() == L'\\') {
        result.pop_back();
    }
    return result;
}

static bool CheckFileExists(const std::wstring& path) {
    DWORD dwAttrib = GetFileAttributesW(path.c_str());
    if (dwAttrib == INVALID_FILE_ATTRIBUTES || (dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) return false;
    LARGE_INTEGER size = {};
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    const bool hasContent = GetFileSizeEx(h, &size) && size.QuadPart > 0;
    CloseHandle(h);
    return hasContent;
}

static bool IsZipArchive(const std::wstring& path) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    BYTE signature[4] = {};
    DWORD read = 0;
    const bool valid = ReadFile(h, signature, sizeof(signature), &read, NULL) && read >= 2 &&
        signature[0] == 0x50 && signature[1] == 0x4B; // ZIP/JAR local or central directory header
    CloseHandle(h);
    return valid;
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

static void EnsureParentDirectory(const std::wstring& path) {
    const size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos) EnsureDirTree(path.substr(0, slash));
}

static std::wstring GetArtifactKey(const std::wstring& path) {
    std::wstring p = path;
    for (auto& c : p) if (c == L'/') c = L'\\';

    size_t s1 = p.find_last_of(L'\\');
    if (s1 == std::wstring::npos) return p;

    size_t s2 = (s1 > 0) ? p.find_last_of(L'\\', s1 - 1) : std::wstring::npos;
    if (s2 == std::wstring::npos) return p.substr(s1 + 1);

    size_t s3 = (s2 > 0) ? p.find_last_of(L'\\', s2 - 1) : std::wstring::npos;
    if (s3 == std::wstring::npos) {
        std::wstring art = p.substr(0, s2);
        std::transform(art.begin(), art.end(), art.begin(), ::towlower);
        return art;
    }

    std::wstring art = p.substr(s3 + 1, s2 - s3 - 1);
    std::transform(art.begin(), art.end(), art.begin(), ::towlower);
    return art;
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
    bool isNativeArchive = false;
};

static std::string JsonStringField(const std::string& json, const char* key) {
    const std::string marker = std::string("\"") + key + "\"";
    const size_t keyPos = json.find(marker);
    if (keyPos == std::string::npos) return "";
    const size_t colon = json.find(':', keyPos + marker.size());
    if (colon == std::string::npos) return "";
    const size_t quote = json.find('"', colon + 1);
    if (quote == std::string::npos) return "";
    bool escaped = false;
    std::string value;
    for (size_t i = quote + 1; i < json.size(); ++i) {
        const char c = json[i];
        if (escaped) { value += c; escaped = false; }
        else if (c == '\\') escaped = true;
        else if (c == '"') return value;
        else value += c;
    }
    return "";
}

static bool ExtractJsonObjectAt(const std::string& json, size_t startAt, std::string& object, size_t* after = nullptr) {
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
        } else if (c == '"') {
            inString = true;
        } else if (c == '{') {
            ++depth;
        } else if (c == '}' && --depth == 0) {
            object = json.substr(begin, i - begin + 1);
            if (after) *after = i + 1;
            return true;
        }
    }
    return false;
}

static std::string JsonObjectField(const std::string& json, const char* key) {
    const std::string marker = std::string("\"") + key + "\"";
    const size_t keyPos = json.find(marker);
    if (keyPos == std::string::npos) return "";
    const size_t colon = json.find(':', keyPos + marker.size());
    if (colon == std::string::npos) return "";
    std::string result;
    return ExtractJsonObjectAt(json, colon + 1, result) ? result : "";
}

static std::string JsonArrayField(const std::string& json, const char* key) {
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
        } else if (c == '"') {
            inString = true;
        } else if (c == '[') {
            ++depth;
        } else if (c == ']' && --depth == 0) {
            return json.substr(open + 1, i - open - 1);
        }
    }
    return "";
}

static void AddLibraryDownload(std::vector<VersionLibInfo>& result, std::unordered_set<std::wstring>& seenPaths,
                               const std::string& object, bool isNativeArchive) {
    const std::string path = JsonStringField(object, "path");
    const std::string url = JsonStringField(object, "url");
    if (path.empty() || url.empty()) return;
    std::wstring widePath(path.begin(), path.end());
    for (wchar_t& c : widePath) if (c == L'/') c = L'\\';
    std::wstring key = widePath;
    std::transform(key.begin(), key.end(), key.begin(), ::towlower);
    if (!seenPaths.insert(key).second) return;
    VersionLibInfo info;
    info.path = widePath;
    info.url.assign(url.begin(), url.end());
    info.isNativeArchive = isNativeArchive;
    result.push_back(std::move(info));
}

// Parse the official Mojang metadata as nested JSON objects instead of scanning
// a fixed 1,200-byte window. In particular, LWJGL's "artifact" jar and its
// "natives-windows" classifier are now distinguished reliably.
static std::vector<VersionLibInfo> ParseLibrariesFromJson(const std::string& json) {
    std::vector<VersionLibInfo> result;
    std::unordered_set<std::wstring> seenPaths;
    const std::string libraries = JsonArrayField(json, "libraries");
    size_t cursor = 0;
    while (!libraries.empty() && cursor < libraries.size()) {
        std::string library;
        size_t after = 0;
        if (!ExtractJsonObjectAt(libraries, cursor, library, &after)) break;
        cursor = after;

        const std::string downloads = JsonObjectField(library, "downloads");
        if (downloads.empty()) continue;
        const std::string artifact = JsonObjectField(downloads, "artifact");
        if (!artifact.empty()) AddLibraryDownload(result, seenPaths, artifact, false);

        const std::string natives = JsonObjectField(library, "natives");
        std::string classifier = JsonStringField(natives, "windows");
        if (classifier.empty()) continue;
        const std::string archToken = "${arch}";
        const size_t arch = classifier.find(archToken);
        if (arch != std::string::npos) classifier.replace(arch, archToken.size(), "64");

        const std::string classifiers = JsonObjectField(downloads, "classifiers");
        const std::string nativeDownload = JsonObjectField(classifiers, classifier.c_str());
        if (!nativeDownload.empty()) AddLibraryDownload(result, seenPaths, nativeDownload, true);
    }
    return result;
}

static std::wstring EscapePowerShellLiteral(const std::wstring& value) {
    std::wstring escaped;
    for (wchar_t c : value) {
        escaped += c;
        if (c == L'\'') escaped += L'\'';
    }
    return escaped;
}

static void ClearExtractedNativeDlls(const std::wstring& nativesDir) {
    WIN32_FIND_DATAW fd;
    HANDLE find = FindFirstFileW((nativesDir + L"\\*.dll").c_str(), &fd);
    if (find == INVALID_HANDLE_VALUE) return;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            DeleteFileW((nativesDir + L"\\" + fd.cFileName).c_str());
        }
    } while (FindNextFileW(find, &fd));
    FindClose(find);
}

static bool ExtractNativeArchive(const std::wstring& archive, const std::wstring& nativesDir) {
    // Native classifier JARs are ZIPs. PowerShell's .NET ZipFile API handles
    // the JAR extension directly and is available on supported Windows builds.
    const std::wstring script =
        L"Add-Type -AssemblyName System.IO.Compression.FileSystem; "
        L"[System.IO.Compression.ZipFile]::ExtractToDirectory('" +
        EscapePowerShellLiteral(archive) + L"','" +
        EscapePowerShellLiteral(nativesDir) + L"',$true)";
    std::wstring command = L"powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command \"" + script + L"\"";
    std::vector<wchar_t> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back(L'\0');
    STARTUPINFOW startup = { sizeof(startup) };
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION process = {};
    if (!CreateProcessW(NULL, mutableCommand.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW,
                        NULL, NULL, &startup, &process)) {
        return false;
    }
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return exitCode == 0;
}

static bool ExtractWindowsNatives(const std::vector<VersionLibInfo>& libraries,
                                  const std::wstring& libDir,
                                  const std::wstring& nativesDir) {
    int nativeArchiveCount = 0;
    ClearExtractedNativeDlls(nativesDir);
    for (const VersionLibInfo& lib : libraries) {
        if (!lib.isNativeArchive) continue;
        ++nativeArchiveCount;
        const std::wstring archive = libDir + L"\\" + lib.path;
        if (!CheckFileExists(archive) || !ExtractNativeArchive(archive, nativesDir)) return false;
    }
    return nativeArchiveCount > 0;
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

static std::wstring GetVersionMetadataUrl(const std::string& manifestJson, const std::wstring& mcVersion) {
    const std::string wantedId(mcVersion.begin(), mcVersion.end());
    const std::string versions = JsonArrayField(manifestJson, "versions");
    size_t cursor = 0;
    while (!versions.empty() && cursor < versions.size()) {
        std::string item;
        size_t after = 0;
        if (!ExtractJsonObjectAt(versions, cursor, item, &after)) break;
        cursor = after;
        if (JsonStringField(item, "id") != wantedId) continue;
        const std::string url = JsonStringField(item, "url");
        return std::wstring(url.begin(), url.end());
    }
    return L"";
}

static std::wstring GetClientDownloadUrl(const std::string& versionJson) {
    const std::string downloads = JsonObjectField(versionJson, "downloads");
    const std::string client = JsonObjectField(downloads, "client");
    const std::string url = JsonStringField(client, "url");
    return std::wstring(url.begin(), url.end());
}

static bool DownloadClientJarWithFallback(const std::wstring& primaryUrl, const std::wstring& destination,
                                          std::function<void(int)> progress) {
    if (!primaryUrl.empty() && MinecraftLauncher::DownloadHttpFile(primaryUrl, destination, progress) &&
        IsZipArchive(destination)) {
        return true;
    }
    DeleteFileW(destination.c_str());

    // Piston Data is the canonical location. The legacy Mojang launcher endpoint
    // serves the same immutable object and gives affected networks a second path.
    const std::wstring marker = L"/v1/objects/";
    const size_t objectPos = primaryUrl.find(marker);
    if (objectPos != std::wstring::npos) {
        const std::wstring objectPath = primaryUrl.substr(objectPos + marker.size());
        const std::wstring fallbackUrl = L"https://launcher.mojang.com/v1/objects/" + objectPath;
        if (MinecraftLauncher::DownloadHttpFile(fallbackUrl, destination, progress) && IsZipArchive(destination)) {
            return true;
        }
    }
    DeleteFileW(destination.c_str());
    return false;
}

int MinecraftLauncher::GetRequiredJavaVersion(const std::wstring& mcVersion) {
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
        candidates.push_back(CleanPath(customPath));
    }

    // Check JAVA_HOME
    WCHAR javaHome[MAX_PATH];
    if (GetEnvironmentVariableW(L"JAVA_HOME", javaHome, MAX_PATH) > 0) {
        std::wstring bin = CleanPath(std::wstring(javaHome) + L"\\bin\\java.exe");
        if (CheckFileExists(bin)) candidates.push_back(bin);
    }

    // Check PATH
    WCHAR pathBuf[MAX_PATH];
    if (SearchPathW(NULL, L"java.exe", NULL, MAX_PATH, pathBuf, NULL) > 0) {
        candidates.push_back(CleanPath(pathBuf));
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
                        std::wstring sub = CleanPath(base + L"\\" + fd.cFileName + L"\\bin\\java.exe");
                        if (CheckFileExists(sub)) candidates.push_back(sub);
                        std::wstring subJre = CleanPath(base + L"\\" + fd.cFileName + L"\\jre\\bin\\java.exe");
                        if (CheckFileExists(subJre)) candidates.push_back(subJre);
                    }
                }
            } while (FindNextFileW(hFind, &fd));
            FindClose(hFind);
        }
    }

    // Inspect first working candidate
    for (const auto& rawCand : candidates) {
        std::wstring cand = CleanPath(rawCand);
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
    HINTERNET hInternet = InternetOpenW(L"Sqikpeek-Aether/1.0.38 (admin@sqikpeek.com)", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return false;

    const wchar_t* customHeaders = L"User-Agent: Sqikpeek-Aether/1.0.38 (admin@sqikpeek.com)\r\nAccept: */*\r\n";
    HINTERNET hUrl = InternetOpenUrlW(
        hInternet,
        url.c_str(),
        customHeaders,
        (DWORD)wcslen(customHeaders),
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE | INTERNET_FLAG_SECURE,
        0
    );

    if (!hUrl) {
        InternetCloseHandle(hInternet);
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    DWORD statusIndex = 0;
    if (!HttpQueryInfoW(hUrl, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                        &statusCode, &statusSize, &statusIndex) ||
        statusCode < 200 || statusCode >= 300) {
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        return false;
    }

    DWORD contentLength = 0;
    DWORD sizeOfLen = sizeof(contentLength);
    DWORD index = 0;
    HttpQueryInfoW(hUrl, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &contentLength, &sizeOfLen, &index);

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
    bool transferOk = true;
    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        DWORD written = 0;
        if (!WriteFile(hFile, buffer, bytesRead, &written, NULL) || written != bytesRead) {
            transferOk = false;
            break;
        }
        totalDownloaded += bytesRead;

        if (contentLength > 0 && progress) {
            int pct = (int)((totalDownloaded * 100ULL) / contentLength);
            progress(pct > 100 ? 100 : pct);
        }
    }

    CloseHandle(hFile);
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);
    if (!transferOk || totalDownloaded == 0) {
        DeleteFileW(destPath.c_str());
        return false;
    }
    return true;
}

bool MinecraftLauncher::PrepareMinecraftFiles(
    const std::wstring& storageDir,
    const std::wstring& mcVersion,
    const std::wstring& loader,
    std::function<void(const std::wstring& status, int percent)> progress
) {
    std::wstring versionDir = storageDir + L"\\versions\\" + mcVersion;
    std::wstring versionJar = versionDir + L"\\" + mcVersion + L".jar";
    std::wstring versionJson = versionDir + L"\\" + mcVersion + L".json";
    EnsureDirTree(versionDir);

    std::wstring libDir = storageDir + L"\\libraries";
    EnsureDirTree(libDir);

    WCHAR appData[MAX_PATH] = { 0 };
    std::wstring mojangGameDir = L"";
    std::wstring mojangLibsDir = L"";
    if (GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH) > 0) {
        mojangGameDir = std::wstring(appData) + L"\\.minecraft";
        mojangLibsDir = mojangGameDir + L"\\libraries";
    }

    // Step 1: Reuse official Launcher files first, then fetch version metadata.
    // This avoids a hard network dependency when the user already owns the game.
    const std::wstring appVersionDir = mojangGameDir.empty() ? L"" :
        mojangGameDir + L"\\versions\\" + mcVersion;
    if (!CheckFileExists(versionJson) && !appVersionDir.empty()) {
        const std::wstring sourceJson = appVersionDir + L"\\" + mcVersion + L".json";
        if (CheckFileExists(sourceJson)) {
            EnsureParentDirectory(versionJson);
            CopyFileW(sourceJson.c_str(), versionJson.c_str(), FALSE);
        }
    }

    if (!CheckFileExists(versionJson)) {
        if (progress) progress(L"Загрузка манифеста версии Minecraft...", 10);
        const std::wstring manifestUrl = L"https://piston-meta.mojang.com/mc/game/version_manifest_v2.json";
        const std::wstring manifestFile = storageDir + L"\\version_manifest_v2.json";
        std::string manifestJson;
        if (CheckFileExists(manifestFile)) {
            if (FILE* file = _wfopen(manifestFile.c_str(), L"rb")) {
                fseek(file, 0, SEEK_END);
                const long size = ftell(file);
                fseek(file, 0, SEEK_SET);
                if (size > 0 && size < 20000000) {
                    std::vector<char> data(size + 1, 0);
                    fread(data.data(), 1, size, file);
                    manifestJson = data.data();
                }
                fclose(file);
            }
        }
        if (GetVersionMetadataUrl(manifestJson, mcVersion).empty()) {
            DeleteFileW(manifestFile.c_str());
            if (MinecraftLauncher::DownloadHttpFile(manifestUrl, manifestFile, nullptr)) {
                if (FILE* file = _wfopen(manifestFile.c_str(), L"rb")) {
                    fseek(file, 0, SEEK_END);
                    const long size = ftell(file);
                    fseek(file, 0, SEEK_SET);
                    if (size > 0 && size < 20000000) {
                        std::vector<char> data(size + 1, 0);
                        fread(data.data(), 1, size, file);
                        manifestJson = data.data();
                    }
                    fclose(file);
                }
            }
        }
        const std::wstring metadataUrl = GetVersionMetadataUrl(manifestJson, mcVersion);
        if (metadataUrl.empty() || !MinecraftLauncher::DownloadHttpFile(metadataUrl, versionJson, nullptr)) {
            DeleteFileW(versionJson.c_str());
            return false;
        }
    }

    std::string versionJsonContent;
    if (FILE* file = _wfopen(versionJson.c_str(), L"rb")) {
        fseek(file, 0, SEEK_END);
        const long size = ftell(file);
        fseek(file, 0, SEEK_SET);
        if (size > 0 && size < 10000000) {
            std::vector<char> data(size + 1, 0);
            fread(data.data(), 1, size, file);
            versionJsonContent = data.data();
        }
        fclose(file);
    }
    const std::wstring clientJarUrl = GetClientDownloadUrl(versionJsonContent);
    if (clientJarUrl.empty()) {
        // A non-empty file is not enough: cached HTML/partial metadata must not
        // cause the invalid old '/1.20.1/client.jar' fallback to be used.
        DeleteFileW(versionJson.c_str());
        return false;
    }

    // Step 2: Reuse or fetch an actual ZIP/JAR client. A failed CDN response is
    // not retained as a fake client.jar and the official launcher mirror is tried.
    if (!IsZipArchive(versionJar) && !appVersionDir.empty()) {
        const std::wstring sourceJar = appVersionDir + L"\\" + mcVersion + L".jar";
        if (IsZipArchive(sourceJar)) {
            DeleteFileW(versionJar.c_str());
            EnsureParentDirectory(versionJar);
            CopyFileW(sourceJar.c_str(), versionJar.c_str(), FALSE);
        }
    }
    if (!IsZipArchive(versionJar)) {
        DeleteFileW(versionJar.c_str());
        if (progress) progress(L"Загрузка официального client.jar...", 25);
        const bool downloaded = DownloadClientJarWithFallback(clientJarUrl, versionJar, [&](int pct) {
            if (progress) {
                const int overall = 25 + (pct * 30) / 100;
                WCHAR statusBuf[128];
                swprintf(statusBuf, 128, L"Загрузка client.jar (%d%%)...", pct);
                progress(statusBuf, overall);
            }
        });
        if (!downloaded || !IsZipArchive(versionJar)) return false;
    }

    // Step 2.5: Ensure sound asset index is prepared (Fixes in-game sound!)
    std::wstring assetIndexId = mcVersion;
    std::wstring assetIndexUrl = L"";
    if (!versionJsonContent.empty()) {
        size_t aPos = versionJsonContent.find("\"assetIndex\":");
        if (aPos != std::string::npos) {
            size_t idPos = versionJsonContent.find("\"id\": \"", aPos);
            if (idPos != std::string::npos && idPos < aPos + 200) {
                idPos += 7;
                size_t idEnd = versionJsonContent.find("\"", idPos);
                if (idEnd != std::string::npos) {
                    std::string aid = versionJsonContent.substr(idPos, idEnd - idPos);
                    assetIndexId = std::wstring(aid.begin(), aid.end());
                }
            }
            size_t uPos = versionJsonContent.find("\"url\": \"", aPos);
            if (uPos != std::string::npos && uPos < aPos + 300) {
                uPos += 8;
                size_t uEnd = versionJsonContent.find("\"", uPos);
                if (uEnd != std::string::npos) {
                    std::string au = versionJsonContent.substr(uPos, uEnd - uPos);
                    assetIndexUrl = std::wstring(au.begin(), au.end());
                }
            }
        }
    }
    if (assetIndexId == mcVersion) {
        int minor = 0;
        swscanf(mcVersion.c_str(), L"1.%d", &minor);
        if (minor >= 21) assetIndexId = L"17";
        else if (minor == 20) assetIndexId = L"16";
        else if (minor == 19) assetIndexId = L"8";
        else if (minor == 18) assetIndexId = L"1.18";
        else if (minor == 17) assetIndexId = L"1.17";
        else if (minor == 16) assetIndexId = L"1.16";
        else if (minor == 12) assetIndexId = L"1.12";
        else assetIndexId = L"legacy";
    }

    std::wstring assetIndexesDir = storageDir + L"\\assets\\indexes";
    EnsureDirTree(assetIndexesDir);
    std::wstring localIndexFile = assetIndexesDir + L"\\" + assetIndexId + L".json";

    if (!CheckFileExists(localIndexFile)) {
        bool foundInAppData = false;
        if (!mojangLibsDir.empty()) {
            WCHAR appDataAssets[MAX_PATH] = { 0 };
            if (GetEnvironmentVariableW(L"APPDATA", appDataAssets, MAX_PATH) > 0) {
                std::wstring appDataIndex = std::wstring(appDataAssets) + L"\\.minecraft\\assets\\indexes\\" + assetIndexId + L".json";
                if (CheckFileExists(appDataIndex)) {
                    CopyFileW(appDataIndex.c_str(), localIndexFile.c_str(), FALSE);
                    foundInAppData = true;
                }
            }
        }
        if (!foundInAppData && !assetIndexUrl.empty()) {
            DownloadHttpFile(assetIndexUrl, localIndexFile, nullptr);
        }
    }

    // Step 2.6: If Fabric loader is selected, ensure Fabric libraries are present!
    if (loader == L"fabric") {
        if (progress) progress(L"Подготовка библиотек Fabric...", 50);
        std::vector<VersionLibInfo> fabricLibs = {
            { L"net\\fabricmc\\fabric-loader\\0.16.10\\fabric-loader-0.16.10.jar",
              L"https://maven.fabricmc.net/net/fabricmc/fabric-loader/0.16.10/fabric-loader-0.16.10.jar" },
            { L"net\\fabricmc\\intermediary\\" + mcVersion + L"\\intermediary-" + mcVersion + L".jar",
              L"https://maven.fabricmc.net/net/fabricmc/intermediary/" + mcVersion + L"/intermediary-" + mcVersion + L".jar" },
            { L"net\\fabricmc\\sponge-mixin\\0.15.3+mixin.0.8.7\\sponge-mixin-0.15.3+mixin.0.8.7.jar",
              L"https://maven.fabricmc.net/net/fabricmc/sponge-mixin/0.15.3+mixin.0.8.7/sponge-mixin-0.15.3+mixin.0.8.7.jar" },
            { L"net\\fabricmc\\tiny-mappings-parser\\0.3.0+build.17\\tiny-mappings-parser-0.3.0+build.17.jar",
              L"https://maven.fabricmc.net/net/fabricmc/tiny-mappings-parser/0.3.0+build.17/tiny-mappings-parser-0.3.0+build.17.jar" },
            { L"net\\fabricmc\\tiny-remapper\\0.10.4\\tiny-remapper-0.10.4.jar",
              L"https://maven.fabricmc.net/net/fabricmc/tiny-remapper/0.10.4/tiny-remapper-0.10.4.jar" },
            { L"net\\fabricmc\\access-widener\\2.1.0\\access-widener-2.1.0.jar",
              L"https://maven.fabricmc.net/net/fabricmc/access-widener/2.1.0/access-widener-2.1.0.jar" },
            { L"net\\fabricmc\\mapping-io\\0.6.1\\mapping-io-0.6.1.jar",
              L"https://maven.fabricmc.net/net/fabricmc/mapping-io/0.6.1/mapping-io-0.6.1.jar" },
            { L"org\\ow2\\asm\\asm\\9.7.1\\asm-9.7.1.jar",
              L"https://maven.fabricmc.net/org/ow2/asm/asm/9.7.1/asm-9.7.1.jar" },
            { L"org\\ow2\\asm\\asm-analysis\\9.7.1\\asm-analysis-9.7.1.jar",
              L"https://maven.fabricmc.net/org/ow2/asm/asm-analysis/9.7.1/asm-analysis-9.7.1.jar" },
            { L"org\\ow2\\asm\\asm-commons\\9.7.1\\asm-commons-9.7.1.jar",
              L"https://maven.fabricmc.net/org/ow2/asm/asm-commons/9.7.1/asm-commons-9.7.1.jar" },
            { L"org\\ow2\\asm\\asm-tree\\9.7.1\\asm-tree-9.7.1.jar",
              L"https://maven.fabricmc.net/org/ow2/asm/asm-tree/9.7.1/asm-tree-9.7.1.jar" },
            { L"org\\ow2\\asm\\asm-util\\9.7.1\\asm-util-9.7.1.jar",
              L"https://maven.fabricmc.net/org/ow2/asm/asm-util/9.7.1/asm-util-9.7.1.jar" }
        };

        for (const auto& flib : fabricLibs) {
            std::wstring targetPath = libDir + L"\\" + flib.path;
            if (!CheckFileExists(targetPath)) {
                bool found = false;
                if (!mojangLibsDir.empty()) {
                    std::wstring appDataLib = mojangLibsDir + L"\\" + flib.path;
                    if (CheckFileExists(appDataLib)) {
                        EnsureParentDirectory(targetPath);
                        found = CopyFileW(appDataLib.c_str(), targetPath.c_str(), FALSE) != FALSE;
                    }
                }
                if (!found && !flib.url.empty()) {
                    found = DownloadHttpFile(flib.url, targetPath, nullptr);
                }
                if (!found || !CheckFileExists(targetPath)) return false;
            }
        }
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
            bool foundInAppData = false;
            if (!mojangLibsDir.empty()) {
                std::wstring appDataLibPath = mojangLibsDir + L"\\" + lib.path;
                if (CheckFileExists(appDataLibPath)) {
                    EnsureParentDirectory(targetPath);
                    foundInAppData = CopyFileW(appDataLibPath.c_str(), targetPath.c_str(), FALSE) != FALSE;
                }
            }

            if (!foundInAppData && !lib.url.empty()) {
                foundInAppData = DownloadHttpFile(lib.url, targetPath, nullptr);
            }
            // Do not launch with a partial classpath. This converts a later
            // NoClassDefFoundError into a clear preparation failure.
            if (!foundInAppData || !CheckFileExists(targetPath)) return false;
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
    HANDLE* outProcessHandle,
    const std::wstring& instanceDirOverride
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

    // 2. Prepare Game Files (client.jar, Mojang libraries, Fabric loader)
    if (callbacks.onProgress) callbacks.onProgress(L"Подготовка файлов игры и библиотек...", 15);
    bool filesOk = PrepareMinecraftFiles(storageDir, mcVersion, loader, callbacks.onProgress);
    if (!filesOk) {
        if (callbacks.onLog) callbacks.onLog(L"[Aether Launcher] Ошибка: Не удалось подготовить файлы для версии " + mcVersion, true);
        if (callbacks.onProgress) callbacks.onProgress(L"Ошибка загрузки файлов", 0);
        return false;
    }

    // 3. Assemble directories and paths (Container isolation)
    std::wstring instanceDir = instanceDirOverride.empty() ? (storageDir + L"\\instances\\" + mcVersion) : instanceDirOverride;
    std::wstring assetsDir = storageDir + L"\\assets";
    std::wstring nativesDir = storageDir + L"\\versions\\" + mcVersion + L"\\natives";
    std::wstring clientJar = storageDir + L"\\versions\\" + mcVersion + L"\\" + mcVersion + L".jar";

    EnsureDirTree(storageDir + L"\\instances");
    EnsureDirTree(instanceDir);
    EnsureDirTree(instanceDir + L"\\mods");
    EnsureDirTree(instanceDir + L"\\screenshots");
    EnsureDirTree(instanceDir + L"\\saves");
    EnsureDirTree(instanceDir + L"\\config");
    EnsureDirTree(assetsDir);
    EnsureDirTree(nativesDir);

    // Native LWJGL archives are not classpath jars. Extract the Windows DLLs
    // before starting Java, otherwise -Djava.library.path points at an empty
    // directory and rendering crashes later in startup.
    const std::wstring versionMetadataPath = storageDir + L"\\versions\\" + mcVersion + L"\\" + mcVersion + L".json";
    std::string versionMetadata;
    if (FILE* metadataFile = _wfopen(versionMetadataPath.c_str(), L"rb")) {
        fseek(metadataFile, 0, SEEK_END);
        const long size = ftell(metadataFile);
        fseek(metadataFile, 0, SEEK_SET);
        if (size > 0 && size < 10000000) {
            std::vector<char> data(size + 1, 0);
            fread(data.data(), 1, size, metadataFile);
            versionMetadata = data.data();
        }
        fclose(metadataFile);
    }
    const std::vector<VersionLibInfo> metadataLibraries = ParseLibrariesFromJson(versionMetadata);
    const std::wstring localLibrariesDir = storageDir + L"\\libraries";
    if (metadataLibraries.empty() || !ExtractWindowsNatives(metadataLibraries, localLibrariesDir, nativesDir)) {
        if (callbacks.onLog) callbacks.onLog(
            L"[Aether Launcher] Не удалось подготовить Windows native-библиотеки LWJGL.", true);
        if (callbacks.onProgress) callbacks.onProgress(L"Ошибка подготовки LWJGL natives", 0);
        return false;
    }

    // Audio / Sound: Check for complete sound objects in %APPDATA%\.minecraft\assets
    std::wstring effectiveAssetsDir = assetsDir;
    WCHAR appDataAssets[MAX_PATH] = { 0 };
    if (GetEnvironmentVariableW(L"APPDATA", appDataAssets, MAX_PATH) > 0) {
        std::wstring mojangAssets = std::wstring(appDataAssets) + L"\\.minecraft\\assets";
        if (PathFileExistsW((mojangAssets + L"\\indexes").c_str()) && PathFileExistsW((mojangAssets + L"\\objects").c_str())) {
            effectiveAssetsDir = mojangAssets;
        }
    }

    // 4. Build Complete Classpath Recursively (Resolving joptsimple, guava, lwjgl, etc.)
    std::vector<std::wstring> allJars;

    // Scan storageDir/libraries recursively
    std::wstring libDir = storageDir + L"\\libraries";
    CollectJarsRecursively(libDir, allJars);

    // Preparation copied/downloaded the exact metadata paths into our storage.
    // Do not mix a second AppData classpath with potentially older artifacts.

    // Client jar first
    std::wstring classpath = L"\"" + clientJar + L"\"";
    std::unordered_set<std::wstring> seenJarNames;
    std::unordered_set<std::wstring> seenArtifacts;

    size_t cSlash = clientJar.find_last_of(L"\\/");
    if (cSlash != std::wstring::npos) {
        std::wstring cName = clientJar.substr(cSlash + 1);
        std::transform(cName.begin(), cName.end(), cName.begin(), ::towlower);
        seenJarNames.insert(cName);
    }
    seenArtifacts.insert(L"client");

    size_t addedCount = 0;
    for (const auto& jarPath : allJars) {
        size_t lastSlash = jarPath.find_last_of(L"\\/");
        std::wstring fname = (lastSlash == std::wstring::npos) ? jarPath : jarPath.substr(lastSlash + 1);
        std::wstring lowerName = fname;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::towlower);

        // Classifier archives contain DLLs, not Java classes. Including one
        // first can make artifact de-duplication discard lwjgl-*.jar and cause
        // NoClassDefFoundError: org/lwjgl/system/MemoryUtil.
        if (lowerName.find(L"-natives-") != std::wstring::npos) continue;
        if ((loader == L"fabric" || loader == L"quilt") &&
            lowerName.find(L"fabric-loader-") == 0 && lowerName != L"fabric-loader-0.16.10.jar") {
            // A previous launcher run may have left an obsolete loader in the
            // shared library tree. Never let directory enumeration select it.
            continue;
        }

        std::wstring artKey = GetArtifactKey(jarPath);

        // Deduplicate both by exact jar filename and by maven artifact ID (e.g. prevent duplicate fabric-loader or mixin)!
        if (seenJarNames.find(lowerName) == seenJarNames.end() &&
            (artKey.empty() || seenArtifacts.find(artKey) == seenArtifacts.end())) {
            seenJarNames.insert(lowerName);
            if (!artKey.empty()) seenArtifacts.insert(artKey);
            classpath += L";\"" + jarPath + L"\"";
            addedCount++;
        }
    }

    if (callbacks.onLog) {
        WCHAR cpMsg[256];
        swprintf(cpMsg, 256, L"[Aether Launcher] Собрано библиотек в classpath: %zu jar-файлов", addedCount);
        callbacks.onLog(cpMsg, false);
    }

    // Determine Main Class and Asset Index
    std::wstring mainClass = L"net.minecraft.client.main.Main";
    if (loader == L"fabric" || loader == L"quilt") {
        mainClass = L"net.fabricmc.loader.impl.launch.knot.KnotClient";
    }

    std::wstring assetIndexId = mcVersion;
    int mcMinor = 0;
    swscanf(mcVersion.c_str(), L"1.%d", &mcMinor);
    if (mcMinor >= 21) assetIndexId = L"17";
    else if (mcMinor == 20) assetIndexId = L"16";
    else if (mcMinor == 19) assetIndexId = L"8";
    else if (mcMinor == 18) assetIndexId = L"1.18";
    else if (mcMinor == 17) assetIndexId = L"1.17";
    else if (mcMinor == 16) assetIndexId = L"1.16";
    else if (mcMinor == 12) assetIndexId = L"1.12";

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
            if (loader == L"vanilla") {
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
            size_t aPos = content.find("\"assetIndex\":");
            if (aPos != std::string::npos) {
                size_t idPos = content.find("\"id\": \"", aPos);
                if (idPos != std::string::npos && idPos < aPos + 200) {
                    idPos += 7;
                    size_t idEnd = content.find("\"", idPos);
                    if (idEnd != std::string::npos) {
                        std::string aid = content.substr(idPos, idEnd - idPos);
                        assetIndexId = std::wstring(aid.begin(), aid.end());
                    }
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
    std::wstring versionType = (loader == L"vanilla") ? L"release" : loader;

    std::wstring cmdLine = L"\"" + java.path + L"\" " +
        memArg + L" " +
        (flagsToUse.empty() ? L"" : (flagsToUse + L" ")) +
        L"-Djava.library.path=\"" + nativesDir + L"\" " +
        L"-Dminecraft.launcher.brand=minecraft " +
        L"-Dminecraft.launcher.version=1.0.38 " +
        L"-cp " + classpath + L" " +
        mainClass + L" " +
        L"--username \"" + finalNickname + L"\" " +
        L"--version \"" + mcVersion + L"\" " +
        L"--gameDir \"" + instanceDir + L"\" " +
        L"--assetsDir \"" + effectiveAssetsDir + L"\" " +
        L"--assetIndex \"" + assetIndexId + L"\" " +
        L"--uuid 00000000-0000-0000-0000-000000000000 " +
        L"--accessToken 0 " +
        L"--userType mojang " +
        L"--versionType " + versionType;

    if (callbacks.onLog) {
        callbacks.onLog(L"[Aether Launcher] JVM Аргументы: " + (flagsToUse.empty() ? L"(по умолчанию)" : flagsToUse), false);
        callbacks.onLog(L"[Aether Launcher] Главный класс клиента: " + mainClass, false);
        callbacks.onLog(L"[Aether Launcher] Индекс аудиоресурсов: " + assetIndexId, false);
        callbacks.onLog(L"[Aether Launcher] Папка экземпляра игры: " + instanceDir, false);
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
    if (logText.find(L"duplicate fabric loader classes") != std::wstring::npos) {
        return L"Обнаружены конфликтующие версии загрузчика Fabric на classpath. Лаунчер автоматически устраняет дубликаты загрузчиков.";
    }
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
