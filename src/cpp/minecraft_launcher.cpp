#include "minecraft_launcher.hpp"
#include <shlwapi.h>
#include <shlobj.h>
#include <wincrypt.h>
#include <stdio.h>
#include <algorithm>
#include <cwctype>
#include <unordered_set>
#include <set>

// Zig's Windows SDK ships mutually incompatible WinINet and WinHTTP headers.
// Declare the small WinHTTP surface used for downloader fallback directly so
// the native launcher still links against the real system winhttp.dll.
extern "C" {
BOOL WINAPI WinHttpCloseHandle(HINTERNET);
HINTERNET WINAPI WinHttpConnect(HINTERNET, LPCWSTR, INTERNET_PORT, DWORD);
HINTERNET WINAPI WinHttpOpen(LPCWSTR, DWORD, LPCWSTR, LPCWSTR, DWORD);
HINTERNET WINAPI WinHttpOpenRequest(HINTERNET, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR*, DWORD);
BOOL WINAPI WinHttpQueryDataAvailable(HINTERNET, LPDWORD);
BOOL WINAPI WinHttpQueryHeaders(HINTERNET, DWORD, LPCWSTR, LPVOID, LPDWORD, LPDWORD);
BOOL WINAPI WinHttpReadData(HINTERNET, LPVOID, DWORD, LPDWORD);
BOOL WINAPI WinHttpReceiveResponse(HINTERNET, LPVOID);
BOOL WINAPI WinHttpSendRequest(HINTERNET, LPCWSTR, DWORD, LPVOID, DWORD, DWORD, DWORD_PTR);
BOOL WINAPI WinHttpSetTimeouts(HINTERNET, int, int, int, int);
}
static constexpr DWORD AETHER_WINHTTP_ACCESS_DEFAULT_PROXY = 0;
static constexpr DWORD AETHER_WINHTTP_ACCESS_NO_PROXY = 1;
static constexpr DWORD AETHER_WINHTTP_FLAG_SECURE = 0x00800000;
static constexpr DWORD AETHER_WINHTTP_QUERY_CONTENT_LENGTH = 5;
static constexpr DWORD AETHER_WINHTTP_QUERY_STATUS_CODE = 19;
static constexpr DWORD AETHER_WINHTTP_QUERY_FLAG_NUMBER = 0x20000000;

static std::wstring FormatWin32Error(DWORD error);
static std::wstring DecodeProcessOutput(const std::string& bytes);
static void ReportCapturedProcessOutput(
    const std::function<void(const std::wstring&)>& onDiagnostic,
    const std::wstring& prefix,
    const std::string& output
);
struct VersionLibInfo;
static bool IsValidLibraryArtifact(const VersionLibInfo& library, const std::wstring& path,
                                   std::wstring* failureReason);
static bool FileMatchesSha1(const std::wstring& path, const std::wstring& expectedHex);

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "crypt32.lib")

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

// Remove stale/read-only artifacts before copying or atomically finalizing a
// download. DeleteFileW alone fails with ERROR_ACCESS_DENIED on READONLY JARs.
static bool RemoveExistingFileForReplacement(const std::wstring& path,
                                             DWORD* errorCode = nullptr,
                                             bool* readOnlyCleared = nullptr,
                                             const wchar_t** failureStage = nullptr,
                                             DWORD* observedAttributes = nullptr) {
    if (errorCode) *errorCode = ERROR_SUCCESS;
    if (readOnlyCleared) *readOnlyCleared = false;
    if (failureStage) *failureStage = L"";
    if (observedAttributes) *observedAttributes = INVALID_FILE_ATTRIBUTES;
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        const DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) return true;
        if (errorCode) *errorCode = error;
        if (failureStage) *failureStage = L"GetFileAttributesW";
        return false;
    }
    if (observedAttributes) *observedAttributes = attributes;
    if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
        if (errorCode) *errorCode = ERROR_ACCESS_DENIED;
        if (failureStage) *failureStage = L"destination is a directory";
        return false;
    }

    const bool wasReadOnly = (attributes & FILE_ATTRIBUTE_READONLY) != 0;
    if (wasReadOnly) {
        DWORD writableAttributes = attributes & ~FILE_ATTRIBUTE_READONLY;
        if (writableAttributes == 0) writableAttributes = FILE_ATTRIBUTE_NORMAL;
        if (!SetFileAttributesW(path.c_str(), writableAttributes)) {
            if (errorCode) *errorCode = GetLastError();
            if (failureStage) *failureStage = L"SetFileAttributesW(clear read-only)";
            return false;
        }
    }
    if (!DeleteFileW(path.c_str())) {
        const DWORD error = GetLastError();
        if (wasReadOnly) SetFileAttributesW(path.c_str(), attributes);
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) return true;
        if (errorCode) *errorCode = error;
        if (failureStage) *failureStage = L"DeleteFileW";
        return false;
    }
    if (readOnlyCleared) *readOnlyCleared = wasReadOnly;
    return true;
}

static std::wstring GetRecoveredLibraryPath(const std::wstring& libraryDir,
                                           const std::wstring& mcVersion,
                                           const std::wstring& loader,
                                           const std::wstring& relativePath) {
    // Keep recovery outside the shared libraries directory so a restrictive
    // ACL on that cache cannot prevent Aether from writing a verified copy.
    const size_t separator = libraryDir.find_last_of(L"\\/");
    const std::wstring storageDir = separator == std::wstring::npos
        ? libraryDir
        : libraryDir.substr(0, separator);
    return storageDir + L"\\.aether-recovered\\libraries\\" + mcVersion + L"\\" +
           loader + L"\\" + relativePath;
}

static bool MoveDownloadedFileIntoPlace(const std::wstring& partialPath,
                                       const std::wstring& destinationPath,
                                       DWORD* errorCode,
                                       const wchar_t** failureStage) {
    if (errorCode) *errorCode = ERROR_SUCCESS;
    if (failureStage) *failureStage = L"";
    for (int attempt = 0; attempt < 4; ++attempt) {
        DWORD cleanupError = ERROR_SUCCESS;
        const wchar_t* cleanupStage = L"";
        if (!RemoveExistingFileForReplacement(destinationPath, &cleanupError, nullptr, &cleanupStage)) {
            if ((cleanupError == ERROR_ACCESS_DENIED || cleanupError == ERROR_SHARING_VIOLATION) && attempt < 3) {
                Sleep(75 * (attempt + 1));
                continue;
            }
            if (errorCode) *errorCode = cleanupError;
            if (failureStage) *failureStage = cleanupStage[0] ? cleanupStage : L"prepare destination for finalization";
            return false;
        }
        if (MoveFileExW(partialPath.c_str(), destinationPath.c_str(),
                        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            return true;
        }
        const DWORD error = GetLastError();
        if (errorCode) *errorCode = error;
        if (error != ERROR_ACCESS_DENIED && error != ERROR_SHARING_VIOLATION) {
            if (failureStage) *failureStage = L"MoveFileExW(finalize)";
            return false;
        }
        if (attempt < 3) Sleep(75 * (attempt + 1));
        else if (failureStage) *failureStage = L"MoveFileExW(finalize) after transient-error retries";
    }
    return false;
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
            // Recovery artifacts are selected separately after validating the
            // matching metadata entry; never sweep this private cache implicitly.
            if (_wcsicmp(fd.cFileName, L".aether-recovered") != 0) {
                CollectJarsRecursively(fullPath, outJars);
            }
        } else {
            size_t len = wcslen(fd.cFileName);
            if (len > 4 && _wcsicmp(fd.cFileName + len - 4, L".jar") == 0 && IsZipArchive(fullPath)) {
                outJars.push_back(fullPath);
            }
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
}

struct VersionLibInfo {
    std::wstring path;
    std::wstring url;
    // Mojang metadata gives immutable library hashes. They allow a Piston Data
    // retry when a regional CDN/proxy cannot reach libraries.minecraft.net.
    std::wstring sha1;
    bool isNativeArchive = false;
};

static std::vector<VersionLibInfo> GetFabricLibraryArtifacts(const std::wstring& mcVersion) {
    return {
        { L"net\\fabricmc\\intermediary\\" + mcVersion + L"\\intermediary-" + mcVersion + L".jar",
          L"https://maven.fabricmc.net/net/fabricmc/intermediary/" + mcVersion + L"/intermediary-" + mcVersion + L".jar",
          mcVersion == L"1.20.1" ? L"97d0bff94981e37bd7a4362deee53c9a84e3fb21" : L"" },
        { L"org\\ow2\\asm\\asm\\9.7.1\\asm-9.7.1.jar",
          L"https://maven.fabricmc.net/org/ow2/asm/asm/9.7.1/asm-9.7.1.jar", L"f0ed132a49244b042cd0e15702ab9f2ce3cc8436" },
        { L"org\\ow2\\asm\\asm-analysis\\9.7.1\\asm-analysis-9.7.1.jar",
          L"https://maven.fabricmc.net/org/ow2/asm/asm-analysis/9.7.1/asm-analysis-9.7.1.jar", L"f97a3b319f0ed6a8cd944dc79060d3912a28985f" },
        { L"org\\ow2\\asm\\asm-commons\\9.7.1\\asm-commons-9.7.1.jar",
          L"https://maven.fabricmc.net/org/ow2/asm/asm-commons/9.7.1/asm-commons-9.7.1.jar", L"406c6a2225cfe1819f102a161e54cc16a5c24f75" },
        { L"org\\ow2\\asm\\asm-tree\\9.7.1\\asm-tree-9.7.1.jar",
          L"https://maven.fabricmc.net/org/ow2/asm/asm-tree/9.7.1/asm-tree-9.7.1.jar", L"3a53139787663b139de76b627fca0084ab60d32c" },
        { L"org\\ow2\\asm\\asm-util\\9.7.1\\asm-util-9.7.1.jar",
          L"https://maven.fabricmc.net/org/ow2/asm/asm-util/9.7.1/asm-util-9.7.1.jar", L"9e23359b598ec6b74b23e53110dd5c577adf2243" },
        { L"net\\fabricmc\\sponge-mixin\\0.15.4+mixin.0.8.7\\sponge-mixin-0.15.4+mixin.0.8.7.jar",
          L"https://maven.fabricmc.net/net/fabricmc/sponge-mixin/0.15.4+mixin.0.8.7/sponge-mixin-0.15.4+mixin.0.8.7.jar", L"6a12aacc794f1078458433116e9ed42c1cc98096" },
        { L"net\\fabricmc\\fabric-loader\\0.16.10\\fabric-loader-0.16.10.jar",
          L"https://maven.fabricmc.net/net/fabricmc/fabric-loader/0.16.10/fabric-loader-0.16.10.jar", L"dbd932c2a3641d2c41cf9abfe47e140a5753fa86" }
    };
}

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

static bool IsNativeArchivePath(const std::wstring& relativePath) {
    std::wstring path = relativePath;
    std::transform(path.begin(), path.end(), path.begin(), ::towlower);
    const size_t slash = path.find_last_of(L"\\/");
    const std::wstring name = slash == std::wstring::npos ? path : path.substr(slash + 1);
    const size_t marker = name.find(L"-natives-");
    return marker != std::wstring::npos && name.size() > 4 &&
           name.substr(name.size() - 4) == L".jar";
}

static bool IsWindowsNativeArchivePath(const std::wstring& relativePath) {
    std::wstring path = relativePath;
    std::transform(path.begin(), path.end(), path.begin(), ::towlower);
    const size_t slash = path.find_last_of(L"\\/");
    const std::wstring name = slash == std::wstring::npos ? path : path.substr(slash + 1);
    const size_t marker = name.find(L"-natives-windows");
    if (marker == std::wstring::npos) return false;
    const std::wstring suffix = name.substr(marker + wcslen(L"-natives-windows"));
    // Do not try to load Windows ARM64/x86 classifier archives on the x64
    // runtime. Mojang uses the unsuffixed JAR (or -64) for Windows x64.
    return suffix == L".jar" || suffix == L"-64.jar" || suffix == L"-x86_64.jar";
}

static void AddLibraryDownload(std::vector<VersionLibInfo>& result, std::unordered_set<std::wstring>& seenPaths,
                               const std::string& object, bool isNativeArchive) {
    const std::string path = JsonStringField(object, "path");
    const std::string url = JsonStringField(object, "url");
    const std::string sha1 = JsonStringField(object, "sha1");
    if (path.empty() || url.empty()) return;
    std::wstring widePath(path.begin(), path.end());
    for (wchar_t& c : widePath) if (c == L'/') c = L'\\';
    std::wstring key = widePath;
    std::transform(key.begin(), key.end(), key.begin(), ::towlower);
    const bool nativeArchive = isNativeArchive || IsNativeArchivePath(widePath);
    if (!seenPaths.insert(key).second) {
        // An artifact may be encountered before the same file's classifier
        // entry. Preserve the native role when de-duplicating that path.
        for (VersionLibInfo& existing : result) {
            std::wstring existingKey = existing.path;
            std::transform(existingKey.begin(), existingKey.end(), existingKey.begin(), ::towlower);
            if (existingKey == key) {
                existing.isNativeArchive = existing.isNativeArchive || nativeArchive;
                break;
            }
        }
        return;
    }
    VersionLibInfo info;
    info.path = widePath;
    info.url.assign(url.begin(), url.end());
    info.sha1.assign(sha1.begin(), sha1.end());
    // Some version JSONs expose native classifiers as top-level artifact paths
    // instead of downloads.classifiers; infer their role from the Maven path.
    info.isNativeArchive = nativeArchive;
    result.push_back(std::move(info));
}

// Parse the official Mojang metadata as nested JSON objects instead of scanning
// a fixed 1,200-byte window. Native role is derived both from the classifiers
// object and from Maven paths, so artifact-style LWJGL native entries are not
// silently left marked as ordinary classpath libraries.
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

        const std::string classifiers = JsonObjectField(downloads, "classifiers");
        const std::string natives = JsonObjectField(library, "natives");
        std::string classifier = JsonStringField(natives, "windows");
        const std::string archToken = "${arch}";
        const size_t arch = classifier.find(archToken);
        if (arch != std::string::npos) classifier.replace(arch, archToken.size(), "64");

        // Honor the declared Windows classifier when available, but also scan
        // classifier download paths. Some Mojang/Fabric metadata lists native
        // JARs as artifact-style entries or omits the natives.windows mapping;
        // selecting by the actual Maven path keeps those archives discoverable.
        if (!classifier.empty()) {
            const std::string nativeDownload = JsonObjectField(classifiers, classifier.c_str());
            if (!nativeDownload.empty()) AddLibraryDownload(result, seenPaths, nativeDownload, true);
        }
        size_t classifierCursor = 0;
        while (!classifiers.empty() && classifierCursor < classifiers.size()) {
            std::string classifierDownload;
            size_t classifierAfter = 0;
            if (!ExtractJsonObjectAt(classifiers, classifierCursor, classifierDownload, &classifierAfter)) break;
            classifierCursor = classifierAfter;
            const std::string classifierPath = JsonStringField(classifierDownload, "path");
            if (classifierPath.empty()) continue;
            const std::wstring wideClassifierPath(classifierPath.begin(), classifierPath.end());
            if (IsWindowsNativeArchivePath(wideClassifierPath)) {
                AddLibraryDownload(result, seenPaths, classifierDownload, true);
            }
        }
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

static bool ExtractNativeArchive(const std::wstring& archive, const std::wstring& nativesDir,
                                 std::function<void(const std::wstring&)> onDiagnostic) {
    // Native classifier JARs are ZIPs. Extract entries individually instead
    // of using ExtractToDirectory(..., $true): older Windows .NET builds bind
    // that third argument as an Encoding and reject the Boolean. Explicitly
    // replace existing entries and reject paths that escape the natives folder.
    const std::wstring script =
        L"Add-Type -AssemblyName System.IO.Compression.FileSystem; "
        L"[Console]::OutputEncoding=[System.Text.Encoding]::UTF8; "
        L"$ErrorActionPreference='Stop'; "
        L"$archivePath='" + EscapePowerShellLiteral(archive) + L"'; "
        L"$destinationPath='" + EscapePowerShellLiteral(nativesDir) + L"'; "
        L"$zip=[System.IO.Compression.ZipFile]::OpenRead($archivePath); "
        L"try { "
        L"$root=[System.IO.Path]::GetFullPath($destinationPath); "
        L"$separator=[string][System.IO.Path]::DirectorySeparatorChar; $prefix=$root; "
        L"if(-not $prefix.EndsWith($separator)) { $prefix+=$separator }; "
        L"$entryCount=0; $dllEntryCount=0; "
        L"foreach($entry in $zip.Entries) { "
        L"$entryCount++; $entryPath=[System.IO.Path]::GetFullPath([System.IO.Path]::Combine($root,$entry.FullName)); "
        L"if(-not $entryPath.StartsWith($prefix,[System.StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe ZIP entry path' }; "
        L"if($entry.FullName.EndsWith('/') -or $entry.FullName.EndsWith('\\')) { [System.IO.Directory]::CreateDirectory($entryPath) | Out-Null; continue }; "
        L"if($entry.FullName.EndsWith('.dll',[System.StringComparison]::OrdinalIgnoreCase)) { "
        L"$target=[System.IO.Path]::Combine($root,[System.IO.Path]::GetFileName($entry.FullName)); $dllEntryCount++; "
        L"} else { $target=$entryPath }; "
        L"$parent=[System.IO.Path]::GetDirectoryName($target); "
        L"if($parent) { [System.IO.Directory]::CreateDirectory($parent) | Out-Null }; "
        L"if([System.IO.File]::Exists($target)) { [System.IO.File]::Delete($target) }; "
        L"[System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$target); "
        L"}; Write-Output ('ZIP entries=' + $entryCount + '; DLL entries=' + $dllEntryCount); "
        L"} finally { $zip.Dispose() }";
    std::wstring command = L"powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command \"" + script + L"\"";
    std::vector<wchar_t> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back(L'\0');

    SECURITY_ATTRIBUTES security = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE outputRead = NULL;
    HANDLE outputWrite = NULL;
    if (!CreatePipe(&outputRead, &outputWrite, &security, 0)) {
        const DWORD error = GetLastError();
        if (onDiagnostic) onDiagnostic(L"[Natives][ERROR] Could not create PowerShell output pipe (Win32 " +
            std::to_wstring(error) + L": " + FormatWin32Error(error) + L").");
        return false;
    }
    if (!SetHandleInformation(outputRead, HANDLE_FLAG_INHERIT, 0)) {
        const DWORD error = GetLastError();
        if (onDiagnostic) onDiagnostic(L"[Natives][ERROR] Could not protect PowerShell read handle (Win32 " +
            std::to_wstring(error) + L": " + FormatWin32Error(error) + L").");
        CloseHandle(outputRead);
        CloseHandle(outputWrite);
        return false;
    }

    HANDLE inputHandle = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (inputHandle == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        if (onDiagnostic) onDiagnostic(L"[Natives][ERROR] Could not open NUL for PowerShell (Win32 " +
            std::to_wstring(error) + L": " + FormatWin32Error(error) + L").");
        CloseHandle(outputRead);
        CloseHandle(outputWrite);
        return false;
    }

    STARTUPINFOW startup = { sizeof(startup) };
    startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    startup.hStdInput = inputHandle;
    startup.hStdOutput = outputWrite;
    startup.hStdError = outputWrite;
    PROCESS_INFORMATION process = {};
    if (onDiagnostic) onDiagnostic(L"[Natives] Starting hidden PowerShell ZIP extraction for " + archive);
    if (!CreateProcessW(NULL, mutableCommand.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW,
                        NULL, NULL, &startup, &process)) {
        const DWORD error = GetLastError();
        if (onDiagnostic) onDiagnostic(L"[Natives][ERROR] PowerShell CreateProcessW failed (Win32 " +
            std::to_wstring(error) + L": " + FormatWin32Error(error) + L").");
        CloseHandle(outputRead);
        CloseHandle(outputWrite);
        CloseHandle(inputHandle);
        return false;
    }
    CloseHandle(outputWrite);
    CloseHandle(inputHandle);

    std::string output;
    bool outputTruncated = false;
    char buffer[2048];
    DWORD bytesRead = 0;
    DWORD readError = ERROR_SUCCESS;
    while (true) {
        if (!ReadFile(outputRead, buffer, sizeof(buffer), &bytesRead, NULL)) {
            readError = GetLastError();
            break;
        }
        if (bytesRead == 0) break;
        const size_t remaining = output.size() < 65536 ? 65536 - output.size() : 0;
        if (remaining > 0) output.append(buffer, std::min<size_t>(bytesRead, remaining));
        if (bytesRead > remaining) outputTruncated = true;
    }
    CloseHandle(outputRead);

    const DWORD waitResult = WaitForSingleObject(process.hProcess, INFINITE);
    const DWORD waitError = waitResult == WAIT_FAILED ? GetLastError() : ERROR_SUCCESS;
    DWORD exitCode = 1;
    const bool exitCodeRead = GetExitCodeProcess(process.hProcess, &exitCode) != FALSE;
    if (waitResult == WAIT_FAILED && onDiagnostic) {
        const DWORD error = waitError;
        onDiagnostic(L"[Natives][ERROR] Waiting for PowerShell failed (Win32 " +
            std::to_wstring(error) + L": " + FormatWin32Error(error) + L").");
    }
    ReportCapturedProcessOutput(onDiagnostic, L"[Natives][PowerShell] ", output);
    if (outputTruncated && onDiagnostic) {
        onDiagnostic(L"[Natives][PowerShell][WARN] PowerShell output exceeded 65536 bytes and was truncated in the log.");
    }
    if (readError != ERROR_SUCCESS && readError != ERROR_BROKEN_PIPE && onDiagnostic) {
        onDiagnostic(L"[Natives][ERROR] Reading PowerShell output failed (Win32 " +
            std::to_wstring(readError) + L": " + FormatWin32Error(readError) + L").");
    }
    if (onDiagnostic) {
        onDiagnostic(L"[Natives] PowerShell exit: wait=" + std::to_wstring(waitResult) +
            L"; exitCode=" + std::to_wstring(exitCode) +
            (exitCodeRead && exitCode == 0 ? L" (success)" : L" (failure)"));
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return waitResult == WAIT_OBJECT_0 && exitCodeRead && exitCode == 0 &&
           (readError == ERROR_SUCCESS || readError == ERROR_BROKEN_PIPE);
}

static bool ExtractWindowsNatives(const std::vector<VersionLibInfo>& libraries,
                                  const std::wstring& libDir,
                                  const std::wstring& nativesDir,
                                  const std::wstring& mcVersion,
                                  const std::wstring& loader,
                                  std::function<void(const std::wstring&)> onDiagnostic) {
    int nativeArchiveCount = 0;
    ClearExtractedNativeDlls(nativesDir);
    if (onDiagnostic) onDiagnostic(L"[Natives] Clearing old DLLs and preparing Windows LWJGL natives in " + nativesDir);
    for (const VersionLibInfo& lib : libraries) {
        // Mojang metadata can represent native JARs as classifier downloads or
        // plain artifacts. Treat the Maven path as authoritative and select
        // only the Windows x64 archive so no platform is silently skipped.
        if (!IsWindowsNativeArchivePath(lib.path)) continue;
        ++nativeArchiveCount;
        if (onDiagnostic) onDiagnostic(L"[Natives] Selected Windows x64 archive: " + lib.path);
        std::wstring archive = libDir + L"\\" + lib.path;
        std::wstring invalidReason;
        if (!IsValidLibraryArtifact(lib, archive, &invalidReason)) {
            const std::wstring recoveredArchive = GetRecoveredLibraryPath(libDir, mcVersion, loader, lib.path);
            std::wstring recoveredReason;
            if (IsValidLibraryArtifact(lib, recoveredArchive, &recoveredReason)) {
                archive = recoveredArchive;
                if (onDiagnostic) onDiagnostic(L"[Natives] Using verified recovery archive: " + archive);
            } else {
                if (onDiagnostic) onDiagnostic(L"[Natives][ERROR] Native archive is invalid at canonical path (" +
                    invalidReason + L") and recovery path (" + recoveredReason + L"): " +
                    libDir + L"\\" + lib.path + L"; recovery=" + recoveredArchive);
                return false;
            }
        }
        if (!ExtractNativeArchive(archive, nativesDir, onDiagnostic)) {
            if (onDiagnostic) onDiagnostic(L"[Natives][ERROR] Failed to extract archive: " + archive);
            return false;
        }
    }
    size_t nativeDllCount = 0;
    WIN32_FIND_DATAW dllData = {};
    HANDLE dllFind = FindFirstFileW((nativesDir + L"\\*.dll").c_str(), &dllData);
    if (dllFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(dllData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) ++nativeDllCount;
        } while (FindNextFileW(dllFind, &dllData));
        FindClose(dllFind);
    }
    if (onDiagnostic) {
        onDiagnostic(L"[Natives] Native archives processed: " + std::to_wstring(nativeArchiveCount) +
                     L"; extracted DLLs=" + std::to_wstring(nativeDllCount));
    }
    return nativeArchiveCount > 0 && nativeDllCount > 0;
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

static bool HasUsableVersionMetadata(const std::wstring& path) {
    if (!CheckFileExists(path)) return false;
    FILE* file = _wfopen(path.c_str(), L"rb");
    if (!file) return false;
    fseek(file, 0, SEEK_END);
    const long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    std::string json;
    if (size > 0 && size < 10000000) {
        std::vector<char> data(size + 1, 0);
        fread(data.data(), 1, size, file);
        json = data.data();
    }
    fclose(file);
    return !GetClientDownloadUrl(json).empty();
}

static std::wstring SanitizeDownloadUrl(const std::wstring& url);

static bool DownloadClientJarWithFallback(
    const std::wstring& primaryUrl,
    const std::wstring& destination,
    std::function<void(int)> progress,
    std::function<void(const std::wstring&)> onDiagnostic
) {
    if (!primaryUrl.empty() && MinecraftLauncher::DownloadHttpFile(primaryUrl, destination, progress, onDiagnostic) &&
        IsZipArchive(destination)) {
        if (onDiagnostic) onDiagnostic(L"[Prepare] Official client.jar downloaded and passed ZIP validation: " + destination);
        return true;
    }
    if (onDiagnostic) onDiagnostic(L"[Prepare] Primary client.jar URL failed or returned an invalid ZIP/JAR: " +
                                   SanitizeDownloadUrl(primaryUrl));
    DeleteFileW(destination.c_str());

    // Piston Data is the canonical location. The legacy Mojang launcher endpoint
    // serves the same immutable object and gives affected networks a second path.
    const std::wstring marker = L"/v1/objects/";
    const size_t objectPos = primaryUrl.find(marker);
    if (objectPos != std::wstring::npos) {
        const std::wstring objectPath = primaryUrl.substr(objectPos + marker.size());
        const std::wstring fallbackUrl = L"https://launcher.mojang.com/v1/objects/" + objectPath;
        if (onDiagnostic) onDiagnostic(L"[Prepare] Retrying client.jar through legacy Mojang CDN: " + fallbackUrl);
        if (MinecraftLauncher::DownloadHttpFile(fallbackUrl, destination, progress, onDiagnostic) && IsZipArchive(destination)) {
            if (onDiagnostic) onDiagnostic(L"[Prepare] Legacy Mojang client.jar passed ZIP validation: " + destination);
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

JavaInfo MinecraftLauncher::DetectJava(
    const std::wstring& customPath,
    int minimumMajor,
    std::function<void(const std::wstring&)> onDiagnostic
) {
    auto diagnostic = [&onDiagnostic](const std::wstring& message) {
        if (onDiagnostic) onDiagnostic(message);
    };
    std::vector<std::wstring> candidates;
    std::wstring explicitCustomCandidatePath;
    auto addCandidate = [&candidates, &diagnostic, &explicitCustomCandidatePath](const std::wstring& rawPath, const wchar_t* source) {
        if (rawPath.empty()) return;
        std::wstring candidate = CleanPath(rawPath);
        std::wstring lower = candidate;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
        const std::wstring suffix = L"javaw.exe";
        if (lower.size() >= suffix.size() && lower.compare(lower.size() - suffix.size(), suffix.size(), suffix) == 0) {
            std::wstring consoleJava = candidate.substr(0, candidate.size() - suffix.size()) + L"java.exe";
            if (CheckFileExists(consoleJava)) {
                diagnostic(L"[Java] Using sibling java.exe instead of javaw.exe to capture runtime diagnostics: " + consoleJava);
                candidate = std::move(consoleJava);
            }
        }
        if (!CheckFileExists(candidate)) {
            diagnostic(L"[Java][WARN] Candidate does not exist or is not readable (" + std::wstring(source) + L"): " + candidate);
            return;
        }
        for (const std::wstring& existing : candidates) {
            if (_wcsicmp(existing.c_str(), candidate.c_str()) == 0) return;
        }
        diagnostic(L"[Java] Added runtime candidate (" + std::wstring(source) + L"): " + candidate);
        if (wcscmp(source, L"custom setting") == 0) explicitCustomCandidatePath = candidate;
        candidates.push_back(std::move(candidate));
    };

    if (!customPath.empty()) {
        diagnostic(L"[Java] Checking user-selected Java path: " + customPath);
        addCandidate(customPath, L"custom setting");
    }

    auto readEnvironmentVariable = [](const wchar_t* name) {
        const DWORD required = GetEnvironmentVariableW(name, NULL, 0);
        if (required == 0 || required > 32768) return std::wstring();
        std::vector<wchar_t> buffer(required);
        const DWORD written = GetEnvironmentVariableW(name, buffer.data(), required);
        if (written == 0 || written >= required) return std::wstring();
        return std::wstring(buffer.data(), written);
    };

    const std::wstring javaHome = readEnvironmentVariable(L"JAVA_HOME");
    if (!javaHome.empty()) {
        diagnostic(L"[Java] JAVA_HOME=" + javaHome);
        addCandidate(CleanPath(javaHome + L"\\bin\\java.exe"), L"JAVA_HOME");
    } else {
        diagnostic(L"[Java] JAVA_HOME is not set.");
    }

    std::vector<wchar_t> pathBuffer(32768, L'\0');
    const DWORD pathLength = SearchPathW(NULL, L"java.exe", NULL,
        static_cast<DWORD>(pathBuffer.size()), pathBuffer.data(), NULL);
    if (pathLength > 0 && pathLength < pathBuffer.size()) {
        addCandidate(std::wstring(pathBuffer.data(), pathLength), L"PATH");
    } else {
        diagnostic(L"[Java] SearchPathW did not find java.exe on PATH.");
    }

    const std::vector<std::wstring> standardDirs = {
        L"C:\\Program Files\\Eclipse Adoptium",
        L"C:\\Program Files\\Java",
        L"C:\\Program Files\\BellSoft",
        L"C:\\Program Files\\Amazon Corretto",
        L"C:\\Program Files\\Microsoft",
        L"C:\\Program Files\\Zulu",
        L"C:\\Program Files (x86)\\Minecraft Launcher\\runtime"
    };
    for (const std::wstring& base : standardDirs) {
        WIN32_FIND_DATAW findData = {};
        const std::wstring pattern = base + L"\\*";
        HANDLE find = FindFirstFileW(pattern.c_str(), &findData);
        if (find == INVALID_HANDLE_VALUE) continue;
        do {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
                wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) continue;
            addCandidate(CleanPath(base + L"\\" + findData.cFileName + L"\\bin\\java.exe"), L"standard install directory");
            addCandidate(CleanPath(base + L"\\" + findData.cFileName + L"\\jre\\bin\\java.exe"), L"standard JRE directory");
        } while (FindNextFileW(find, &findData));
        FindClose(find);
    }

    auto parseMajorVersion = [](const std::string& output) {
        const size_t marker = output.find("version \"");
        if (marker == std::string::npos) return 0;
        size_t position = marker + 9;
        if (output.compare(position, 2, "1.") == 0) position += 2;
        if (position >= output.size() || output[position] < '0' || output[position] > '9') return 0;
        int major = 0;
        while (position < output.size() && output[position] >= '0' && output[position] <= '9') {
            major = major * 10 + (output[position] - '0');
            ++position;
        }
        return major;
    };

    JavaInfo best;
    best.vendor = L"Not found";
    int bestMajor = 0;
    for (size_t candidateIndex = 0; candidateIndex < candidates.size(); ++candidateIndex) {
        const std::wstring& candidate = candidates[candidateIndex];
        const bool explicitCustom = !explicitCustomCandidatePath.empty() &&
            _wcsicmp(candidate.c_str(), explicitCustomCandidatePath.c_str()) == 0;
        diagnostic(L"[Java] Probing runtime: " + candidate);

        SECURITY_ATTRIBUTES security = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
        HANDLE readPipe = NULL;
        HANDLE writePipe = NULL;
        if (!CreatePipe(&readPipe, &writePipe, &security, 0)) {
            const DWORD error = GetLastError();
            diagnostic(L"[Java][ERROR] Could not create version-probe pipe (Win32 " +
                       std::to_wstring(error) + L": " + FormatWin32Error(error) + L").");
            continue;
        }
        if (!SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0)) {
            const DWORD error = GetLastError();
            diagnostic(L"[Java][ERROR] Could not protect version-probe read handle (Win32 " +
                       std::to_wstring(error) + L": " + FormatWin32Error(error) + L").");
            CloseHandle(readPipe);
            CloseHandle(writePipe);
            continue;
        }

        HANDLE inputHandle = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
            &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (inputHandle == INVALID_HANDLE_VALUE) {
            const DWORD error = GetLastError();
            diagnostic(L"[Java][ERROR] Could not open NUL for the version probe (Win32 " +
                       std::to_wstring(error) + L": " + FormatWin32Error(error) + L").");
            CloseHandle(readPipe);
            CloseHandle(writePipe);
            continue;
        }

        STARTUPINFOW startup = { sizeof(STARTUPINFOW) };
        startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        startup.wShowWindow = SW_HIDE;
        startup.hStdInput = inputHandle;
        startup.hStdOutput = writePipe;
        startup.hStdError = writePipe;
        PROCESS_INFORMATION process = {};
        const std::wstring command = L"\"" + candidate + L"\" -version";
        std::vector<wchar_t> mutableCommand(command.begin(), command.end());
        mutableCommand.push_back(L'\0');
        if (!CreateProcessW(candidate.c_str(), mutableCommand.data(), NULL, NULL, TRUE,
                            CREATE_NO_WINDOW, NULL, NULL, &startup, &process)) {
            const DWORD error = GetLastError();
            diagnostic(L"[Java][ERROR] Java version probe could not start (Win32 " +
                       std::to_wstring(error) + L": " + FormatWin32Error(error) +
                       L"); executable=" + candidate);
            CloseHandle(readPipe);
            CloseHandle(writePipe);
            CloseHandle(inputHandle);
            continue;
        }
        CloseHandle(writePipe);
        CloseHandle(inputHandle);

        DWORD waitResult = WaitForSingleObject(process.hProcess, 5000);
        bool timedOut = waitResult == WAIT_TIMEOUT;
        if (timedOut) {
            diagnostic(L"[Java][ERROR] Java -version timed out after 5000 ms; terminating probe: " + candidate);
            TerminateProcess(process.hProcess, ERROR_TIMEOUT);
            WaitForSingleObject(process.hProcess, 1000);
        } else if (waitResult == WAIT_FAILED) {
            const DWORD error = GetLastError();
            diagnostic(L"[Java][ERROR] Waiting for Java version probe failed (Win32 " +
                       std::to_wstring(error) + L": " + FormatWin32Error(error) + L").");
            TerminateProcess(process.hProcess, ERROR_PROCESS_ABORTED);
            WaitForSingleObject(process.hProcess, 1000);
        }

        DWORD exitCode = 1;
        const bool gotExitCode = GetExitCodeProcess(process.hProcess, &exitCode) != FALSE;
        std::string versionOutput;
        char buffer[2048];
        DWORD bytesRead = 0;
        while (ReadFile(readPipe, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
            const size_t remaining = versionOutput.size() < 16384 ? 16384 - versionOutput.size() : 0;
            if (remaining > 0) versionOutput.append(buffer, std::min<size_t>(bytesRead, remaining));
        }
        CloseHandle(readPipe);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);

        const int major = parseMajorVersion(versionOutput);
        if (!versionOutput.empty()) {
            std::wstring output = DecodeProcessOutput(versionOutput);
            if (output.size() > 2000) output.resize(2000);
            diagnostic(L"[Java] -version output: " + output);
        }
        if (timedOut || !gotExitCode || exitCode != 0 || major <= 0) {
            diagnostic(L"[Java][ERROR] Runtime probe failed; wait=" + std::to_wstring(waitResult) +
                       L"; exitCode=" + std::to_wstring(exitCode) +
                       L"; parsed major=" + std::to_wstring(major) + L"; executable=" + candidate);
            continue;
        }

        JavaInfo info;
        info.path = candidate;
        info.majorVersion = major;
        info.vendor = L"Java Runtime";
        info.is64Bit = true;
        info.architectureKnown = false;
        DWORD binaryType = 0;
        if (GetBinaryTypeW(candidate.c_str(), &binaryType)) {
            if (binaryType == SCS_32BIT_BINARY) {
                info.is64Bit = false;
                info.architectureKnown = true;
            } else if (binaryType == SCS_64BIT_BINARY) {
                info.is64Bit = true;
                info.architectureKnown = true;
            }
        }
        if (!info.architectureKnown && versionOutput.find("64-Bit") != std::string::npos) {
            info.is64Bit = true;
            info.architectureKnown = true;
        }
        if (versionOutput.find("Temurin") != std::string::npos) info.vendor = L"Eclipse Temurin";
        else if (versionOutput.find("HotSpot") != std::string::npos) info.vendor = L"OpenJDK HotSpot";
        else if (versionOutput.find("Corretto") != std::string::npos) info.vendor = L"Amazon Corretto";
        else if (versionOutput.find("Zulu") != std::string::npos) info.vendor = L"Azul Zulu";

        diagnostic(L"[Java] Probe succeeded: Java " + std::to_wstring(info.majorVersion) +
                   L"; vendor=" + info.vendor + L"; architecture=" +
                   (info.architectureKnown ? (info.is64Bit ? L"x64" : L"x86") : L"unknown") +
                   L"; path=" + info.path);
        if (explicitCustom || minimumMajor <= 0 || info.majorVersion >= minimumMajor) return info;
        if (info.majorVersion > bestMajor) {
            best = info;
            bestMajor = info.majorVersion;
        }
    }

    if (!best.path.empty()) {
        diagnostic(L"[Java][WARN] No automatic Java candidate met required version " +
                   std::to_wstring(minimumMajor) + L"; highest detected was " +
                   std::to_wstring(best.majorVersion) + L" at " + best.path);
        return best;
    }
    diagnostic(L"[Java][ERROR] No runnable Java installation was detected. Set JAVA_HOME, add java.exe to PATH, or select java.exe in Settings.");
    return best;
}

struct DownloadAttemptDetails {
    DWORD httpStatus = 0;
    DWORD win32Error = ERROR_SUCCESS;
    ULONGLONG bytesReceived = 0;
    ULONGLONG expectedBytes = 0;
    std::wstring stage;
};

static std::wstring FormatWin32Error(DWORD error) {
    if (error == ERROR_SUCCESS) return L"";
    WCHAR message[512] = {};
    const DWORD length = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, error, 0, message, (DWORD)(sizeof(message) / sizeof(message[0])), NULL);
    std::wstring result = length ? std::wstring(message, length) : L"unknown Windows error";
    while (!result.empty() && (result.back() == L'\r' || result.back() == L'\n' || result.back() == L' ')) {
        result.pop_back();
    }
    return result;
}

static void AddSensitiveOutputValue(std::vector<std::wstring>& values, const std::wstring& value) {
    // Tiny values are too common to redact safely from diagnostic text. Minecraft
    // names are at least three characters; tokens and UUIDs are substantially longer.
    if (value.size() < 3) return;
    if (std::find(values.begin(), values.end(), value) == values.end()) values.push_back(value);
}

static void AddSensitiveJvmArguments(const std::wstring& arguments, std::vector<std::wstring>& values) {
    AddSensitiveOutputValue(values, arguments);
    std::wstring current;
    bool inQuotes = false;
    auto flush = [&]() {
        if (current.empty()) return;
        AddSensitiveOutputValue(values, current);
        const size_t equals = current.find(L'=');
        if (equals != std::wstring::npos && equals + 1 < current.size()) {
            AddSensitiveOutputValue(values, current.substr(equals + 1));
        }
        current.clear();
    };
    for (size_t i = 0; i < arguments.size(); ++i) {
        const wchar_t ch = arguments[i];
        if (ch == L'\\' && i + 1 < arguments.size() && arguments[i + 1] == L'"') {
            current.push_back(L'"');
            ++i;
        } else if (ch == L'"') {
            inQuotes = !inQuotes;
        } else if (iswspace(ch) && !inQuotes) {
            flush();
        } else {
            current.push_back(ch);
        }
    }
    flush();
}

static std::wstring RedactSensitiveProcessOutput(
    std::wstring text,
    const std::vector<std::wstring>& sensitiveValues
) {
    static const std::wstring marker = L"[REDACTED]";
    for (const std::wstring& value : sensitiveValues) {
        if (value.size() < 3) continue;
        size_t position = 0;
        while ((position = text.find(value, position)) != std::wstring::npos) {
            text.replace(position, value.size(), marker);
            position += marker.size();
        }
    }
    return text;
}

static std::wstring DecodeProcessOutput(const std::string& bytes) {
    if (bytes.empty()) return L"";
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(),
                                    static_cast<int>(bytes.size()), NULL, 0);
    UINT codePage = CP_UTF8;
    DWORD flags = MB_ERR_INVALID_CHARS;
    if (count <= 0) {
        codePage = CP_ACP;
        flags = 0;
        count = MultiByteToWideChar(codePage, flags, bytes.data(),
                                    static_cast<int>(bytes.size()), NULL, 0);
    }
    if (count <= 0) return std::wstring(bytes.begin(), bytes.end());
    std::wstring result(static_cast<size_t>(count), L'\0');
    MultiByteToWideChar(codePage, flags, bytes.data(), static_cast<int>(bytes.size()),
                        result.data(), count);
    return result;
}

static void ReportCapturedProcessOutput(
    const std::function<void(const std::wstring&)>& onDiagnostic,
    const std::wstring& prefix,
    const std::string& output
) {
    if (!onDiagnostic || output.empty()) return;
    const std::wstring decoded = DecodeProcessOutput(output);
    size_t start = 0;
    while (start < decoded.size()) {
        const size_t end = decoded.find_first_of(L"\r\n", start);
        const size_t length = (end == std::wstring::npos ? decoded.size() : end) - start;
        if (length > 0) onDiagnostic(prefix + decoded.substr(start, length));
        if (end == std::wstring::npos) break;
        start = end + 1;
        if (decoded[end] == L'\r' && start < decoded.size() && decoded[start] == L'\n') ++start;
    }
}

static bool DownloadHttpFileViaWinInet(
    const std::wstring& url,
    const std::wstring& destPath,
    std::function<void(int percent)> progress,
    DownloadAttemptDetails* details = nullptr
) {
    if (details) *details = DownloadAttemptDetails{};
    auto fail = [details](const wchar_t* stage, DWORD error) {
        if (details) {
            details->stage = stage;
            details->win32Error = error;
        }
        return false;
    };

    HINTERNET hInternet = InternetOpenW(L"Sqikpeek-Aether/1.0.38 (admin@sqikpeek.com)", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return fail(L"InternetOpenW", GetLastError());

    const wchar_t* customHeaders = L"User-Agent: Sqikpeek-Aether/1.0.38 (admin@sqikpeek.com)\r\nAccept: */*\r\n";
    const DWORD requestFlags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE |
        (url.rfind(L"https://", 0) == 0 ? INTERNET_FLAG_SECURE : 0);
    HINTERNET hUrl = InternetOpenUrlW(
        hInternet, url.c_str(), customHeaders, (DWORD)wcslen(customHeaders), requestFlags, 0);
    if (!hUrl) {
        const DWORD error = GetLastError();
        InternetCloseHandle(hInternet);
        return fail(L"InternetOpenUrlW", error);
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    DWORD statusIndex = 0;
    if (!HttpQueryInfoW(hUrl, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                        &statusCode, &statusSize, &statusIndex)) {
        const DWORD error = GetLastError();
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        return fail(L"HttpQueryInfoW(status)", error);
    }
    if (details) details->httpStatus = statusCode;
    if (statusCode < 200 || statusCode >= 300) {
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        return fail(L"HTTP response", ERROR_SUCCESS);
    }

    DWORD contentLength = 0;
    DWORD sizeOfLen = sizeof(contentLength);
    DWORD index = 0;
    const bool hasContentLength = HttpQueryInfoW(hUrl,
        HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &contentLength, &sizeOfLen, &index) != FALSE;
    if (details && hasContentLength) details->expectedBytes = contentLength;

    EnsureParentDirectory(destPath);
    const std::wstring partialPath = destPath + L".download";
    DWORD partialCleanupError = ERROR_SUCCESS;
    if (!RemoveExistingFileForReplacement(partialPath, &partialCleanupError)) {
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        return fail(L"remove stale partial", partialCleanupError);
    }
    HANDLE hFile = CreateFileW(partialPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        return fail(L"CreateFileW(partial)", error);
    }

    BYTE buffer[16384];
    DWORD bytesRead = 0;
    ULONGLONG totalDownloaded = 0;
    bool transferOk = true;
    DWORD failureCode = ERROR_SUCCESS;
    const wchar_t* failureStage = L"";
    while (true) {
        if (!InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead)) {
            transferOk = false;
            failureCode = GetLastError();
            failureStage = L"InternetReadFile";
            break;
        }
        if (bytesRead == 0) break;

        DWORD written = 0;
        if (!WriteFile(hFile, buffer, bytesRead, &written, NULL)) {
            transferOk = false;
            failureCode = GetLastError();
            failureStage = L"WriteFile(partial)";
            break;
        }
        if (written != bytesRead) {
            transferOk = false;
            failureCode = ERROR_WRITE_FAULT;
            failureStage = L"WriteFile(partial) returned a short write";
            break;
        }
        totalDownloaded += bytesRead;
        if (details) details->bytesReceived = totalDownloaded;

        if (contentLength > 0 && progress) {
            const int pct = (int)((totalDownloaded * 100ULL) / contentLength);
            progress(pct > 100 ? 100 : pct);
        }
    }
    if (transferOk && contentLength > 0 && totalDownloaded != contentLength) {
        transferOk = false;
        failureCode = ERROR_HANDLE_EOF;
        failureStage = L"Content-Length mismatch";
    }
    if (transferOk && !FlushFileBuffers(hFile)) {
        transferOk = false;
        failureCode = GetLastError();
        failureStage = L"FlushFileBuffers";
    }

    CloseHandle(hFile);
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);
    if (details) {
        details->bytesReceived = totalDownloaded;
        if (hasContentLength) details->expectedBytes = contentLength;
    }
    if (!transferOk || totalDownloaded == 0) {
        DeleteFileW(partialPath.c_str());
        return fail(failureStage[0] ? failureStage : L"empty response",
                    failureStage[0] ? failureCode : ERROR_INVALID_DATA);
    }
    DWORD finalizeError = ERROR_SUCCESS;
    const wchar_t* finalizeStage = L"";
    if (!MoveDownloadedFileIntoPlace(partialPath, destPath, &finalizeError, &finalizeStage)) {
        DeleteFileW(partialPath.c_str());
        return fail(finalizeStage, finalizeError);
    }
    if (details) details->stage = L"completed";
    return true;
}

// WinINet follows the interactive user's IE proxy settings. That is normally
// useful, but it can leave game libraries unreachable when an old or blocked
// proxy is configured. Retry the exact same HTTPS transfer through WinHTTP's
// Windows service proxy stack before rejecting a dependency.
static bool DownloadHttpFileViaWinHttp(
    const std::wstring& url,
    const std::wstring& destPath,
    std::function<void(int percent)> progress,
    DWORD accessType = AETHER_WINHTTP_ACCESS_DEFAULT_PROXY,
    DownloadAttemptDetails* details = nullptr
) {
    if (details) *details = DownloadAttemptDetails{};
    auto fail = [details](const wchar_t* stage, DWORD error) {
        if (details) {
            details->stage = stage;
            details->win32Error = error;
        }
        return false;
    };

    URL_COMPONENTSW parts = {};
    parts.dwStructSize = sizeof(parts);
    WCHAR host[512] = {};
    WCHAR path[4096] = {};
    WCHAR extra[4096] = {};
    parts.lpszHostName = host;
    parts.dwHostNameLength = (DWORD)(sizeof(host) / sizeof(host[0]));
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = (DWORD)(sizeof(path) / sizeof(path[0]));
    parts.lpszExtraInfo = extra;
    parts.dwExtraInfoLength = (DWORD)(sizeof(extra) / sizeof(extra[0]));
    if (!InternetCrackUrlW(url.c_str(), 0, 0, &parts)) return fail(L"InternetCrackUrlW", GetLastError());

    HINTERNET session = WinHttpOpen(L"AetherLauncher/1.0.38", accessType, NULL, NULL, 0);
    if (!session) return fail(L"WinHttpOpen", GetLastError());
    if (!WinHttpSetTimeouts(session, 8000, 8000, 15000, 30000)) {
        const DWORD error = GetLastError();
        WinHttpCloseHandle(session);
        return fail(L"WinHttpSetTimeouts", error);
    }
    HINTERNET connection = WinHttpConnect(session, host, parts.nPort, 0);
    if (!connection) {
        const DWORD error = GetLastError();
        WinHttpCloseHandle(session);
        return fail(L"WinHttpConnect", error);
    }
    std::wstring resource = path;
    resource += extra;
    const DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS ? AETHER_WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connection, L"GET", resource.c_str(), NULL,
                                            NULL, NULL, flags);
    if (!request) {
        const DWORD error = GetLastError();
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return fail(L"WinHttpOpenRequest", error);
    }
    const wchar_t* headers = L"Accept: */*\r\n";
    if (!WinHttpSendRequest(request, headers, (DWORD)-1, NULL, 0, 0, 0)) {
        const DWORD error = GetLastError();
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return fail(L"WinHttpSendRequest", error);
    }
    if (!WinHttpReceiveResponse(request, NULL)) {
        const DWORD error = GetLastError();
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return fail(L"WinHttpReceiveResponse", error);
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    if (!WinHttpQueryHeaders(request, AETHER_WINHTTP_QUERY_STATUS_CODE | AETHER_WINHTTP_QUERY_FLAG_NUMBER,
                             NULL, &status, &statusSize, NULL)) {
        const DWORD error = GetLastError();
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return fail(L"WinHttpQueryHeaders(status)", error);
    }
    if (details) details->httpStatus = status;
    if (status < 200 || status >= 300) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return fail(L"HTTP response", ERROR_SUCCESS);
    }

    std::wstring partial = destPath + L".download";
    EnsureParentDirectory(destPath);
    DWORD partialCleanupError = ERROR_SUCCESS;
    if (!RemoveExistingFileForReplacement(partial, &partialCleanupError)) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return fail(L"remove stale partial", partialCleanupError);
    }
    HANDLE file = CreateFileW(partial.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return fail(L"CreateFileW(partial)", error);
    }

    DWORD contentLength = 0;
    DWORD contentLengthSize = sizeof(contentLength);
    const bool hasContentLength = WinHttpQueryHeaders(request,
        AETHER_WINHTTP_QUERY_CONTENT_LENGTH | AETHER_WINHTTP_QUERY_FLAG_NUMBER,
        NULL, &contentLength, &contentLengthSize, NULL) != FALSE;
    if (details && hasContentLength) details->expectedBytes = contentLength;
    ULONGLONG total = 0;
    bool success = true;
    DWORD failureCode = ERROR_SUCCESS;
    const wchar_t* failureStage = L"";
    while (success) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available)) {
            success = false;
            failureCode = GetLastError();
            failureStage = L"WinHttpQueryDataAvailable";
            break;
        }
        if (available == 0) break;
        std::vector<BYTE> buffer(available);
        DWORD read = 0;
        if (!WinHttpReadData(request, buffer.data(), available, &read)) {
            success = false;
            failureCode = GetLastError();
            failureStage = L"WinHttpReadData";
            break;
        }
        if (read == 0) {
            success = false;
            failureCode = ERROR_HANDLE_EOF;
            failureStage = L"WinHttpReadData returned zero bytes";
            break;
        }
        DWORD written = 0;
        if (!WriteFile(file, buffer.data(), read, &written, NULL)) {
            success = false;
            failureCode = GetLastError();
            failureStage = L"WriteFile(partial)";
            break;
        }
        if (written != read) {
            success = false;
            failureCode = ERROR_WRITE_FAULT;
            failureStage = L"WriteFile(partial) returned a short write";
            break;
        }
        total += read;
        if (details) details->bytesReceived = total;
        if (contentLength > 0 && progress) {
            progress((int)std::min<ULONGLONG>(100, (total * 100ULL) / contentLength));
        }
    }
    if (success && contentLength > 0 && total != contentLength) {
        success = false;
        failureCode = ERROR_HANDLE_EOF;
        failureStage = L"Content-Length mismatch";
    }
    if (success && !FlushFileBuffers(file)) {
        success = false;
        failureCode = GetLastError();
        failureStage = L"FlushFileBuffers";
    }
    CloseHandle(file);
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    if (details) {
        details->bytesReceived = total;
        if (hasContentLength) details->expectedBytes = contentLength;
    }
    if (!success || total == 0) {
        DeleteFileW(partial.c_str());
        return fail(failureStage[0] ? failureStage : L"empty response",
                    failureStage[0] ? failureCode : ERROR_INVALID_DATA);
    }
    DWORD finalizeError = ERROR_SUCCESS;
    const wchar_t* finalizeStage = L"";
    if (!MoveDownloadedFileIntoPlace(partial, destPath, &finalizeError, &finalizeStage)) {
        DeleteFileW(partial.c_str());
        return fail(finalizeStage, finalizeError);
    }
    if (details) details->stage = L"completed";
    return true;
}

static void AddUniqueDownloadUrl(std::vector<std::wstring>& urls, const std::wstring& candidate) {
    if (candidate.empty()) return;
    for (const std::wstring& existing : urls) {
        if (_wcsicmp(existing.c_str(), candidate.c_str()) == 0) return;
    }
    urls.push_back(candidate);
}

static std::vector<std::wstring> GetDownloadUrlCandidates(const std::wstring& url) {
    std::vector<std::wstring> urls;
    AddUniqueDownloadUrl(urls, url);
    const std::wstring librariesPrefix = L"https://libraries.minecraft.net/";
    const std::wstring fabricPrefix = L"https://maven.fabricmc.net/";
    std::wstring suffix;
    if (url.rfind(librariesPrefix, 0) == 0) {
        suffix = url.substr(librariesPrefix.size());
    } else if (url.rfind(fabricPrefix, 0) == 0) {
        // Fabric publishes several dependencies to Maven Central as well. The
        // canonical Fabric repository remains first, then public Maven mirrors
        // are attempted when it is unavailable on the player's network.
        suffix = url.substr(fabricPrefix.size());
    }
    if (!suffix.empty()) {
        // A single Maven Central endpoint is frequently blocked or rate-limited
        // by regional networks. All mirrors below serve the same path; JARs
        // are SHA-1 checked against Mojang/Fabric metadata before use.
        AddUniqueDownloadUrl(urls, L"https://repo.maven.apache.org/maven2/" + suffix);
        AddUniqueDownloadUrl(urls, L"https://repo1.maven.org/maven2/" + suffix);
        AddUniqueDownloadUrl(urls, L"https://maven-central.storage-download.googleapis.com/maven2/" + suffix);
        AddUniqueDownloadUrl(urls, L"https://maven.aliyun.com/repository/central/" + suffix);
        AddUniqueDownloadUrl(urls, L"https://repo.huaweicloud.com/repository/maven/" + suffix);
    }
    return urls;
}

static std::wstring SanitizeDownloadUrl(const std::wstring& url) {
    const size_t query = url.find_first_of(L"?#");
    return query == std::wstring::npos ? url : url.substr(0, query) + L"?[redacted]";
}

static std::wstring DescribeDownloadAttempt(const std::wstring& transport,
                                            const std::wstring& url,
                                            bool success,
                                            const DownloadAttemptDetails& details) {
    std::wstring message = L"[Download] " + transport + (success ? L" succeeded" : L" failed") +
        L"; URL=" + SanitizeDownloadUrl(url);
    if (!details.stage.empty()) message += L"; stage=" + details.stage;
    if (details.httpStatus) message += L"; HTTP=" + std::to_wstring(details.httpStatus);
    if (details.win32Error) {
        message += L"; Win32=" + std::to_wstring(details.win32Error);
        const std::wstring errorText = FormatWin32Error(details.win32Error);
        if (!errorText.empty()) message += L" (" + errorText + L")";
    }
    message += L"; bytes=" + std::to_wstring(details.bytesReceived);
    if (details.expectedBytes) message += L"/" + std::to_wstring(details.expectedBytes);
    return message;
}

bool MinecraftLauncher::DownloadHttpFile(
    const std::wstring& url,
    const std::wstring& destPath,
    std::function<void(int percent)> progress,
    std::function<void(const std::wstring& diagnostic)> onDiagnostic
) {
    const std::vector<std::wstring> candidates = GetDownloadUrlCandidates(url);
    auto prepareDestination = [&]() {
        DWORD error = ERROR_SUCCESS;
        bool readOnlyCleared = false;
        if (!RemoveExistingFileForReplacement(destPath, &error, &readOnlyCleared)) {
            if (onDiagnostic) onDiagnostic(L"[Download][ERROR] Could not remove the existing destination (Win32 " +
                std::to_wstring(error) + L": " + FormatWin32Error(error) + L"): " + destPath);
            return false;
        }
        if (readOnlyCleared && onDiagnostic) {
            onDiagnostic(L"[Download] Cleared the read-only attribute before replacing: " + destPath);
        }
        return true;
    };

    for (const std::wstring& candidate : candidates) {
        if (!prepareDestination()) return false;
        DownloadAttemptDetails details;
        const bool wininetOk = DownloadHttpFileViaWinInet(candidate, destPath, progress, &details);
        if (onDiagnostic) onDiagnostic(DescribeDownloadAttempt(L"WinINet", candidate, wininetOk, details));
        if (wininetOk) return true;

        // A second HTTP implementation avoids a broken user-level WinINet
        // proxy while preserving normal enterprise/Windows proxy behaviour.
        const bool winhttpOk = DownloadHttpFileViaWinHttp(candidate, destPath, progress,
            AETHER_WINHTTP_ACCESS_DEFAULT_PROXY, &details);
        if (onDiagnostic) onDiagnostic(DescribeDownloadAttempt(L"WinHTTP(default proxy)", candidate, winhttpOk, details));
        if (winhttpOk) return true;

        // Last resort: bypass a stale/manual Windows proxy entirely. This is
        // especially useful when Modrinth is reachable but Maven hosts are not.
        const bool directOk = DownloadHttpFileViaWinHttp(candidate, destPath, progress,
            AETHER_WINHTTP_ACCESS_NO_PROXY, &details);
        if (onDiagnostic) onDiagnostic(DescribeDownloadAttempt(L"WinHTTP(no proxy)", candidate, directOk, details));
        if (directOk) return true;
    }
    if (onDiagnostic) onDiagnostic(L"[Download] All configured URLs and HTTP transports failed for " + SanitizeDownloadUrl(url));
    return false;
}

static std::vector<std::wstring> ParseReleaseVersionIds(const std::string& manifestJson) {
    std::vector<std::wstring> versions;
    std::unordered_set<std::wstring> seen;
    const std::string versionEntries = JsonArrayField(manifestJson, "versions");
    size_t cursor = 0;
    while (!versionEntries.empty() && cursor < versionEntries.size()) {
        std::string entry;
        size_t after = 0;
        if (!ExtractJsonObjectAt(versionEntries, cursor, entry, &after)) break;
        cursor = after;
        if (JsonStringField(entry, "type") != "release") continue;
        const std::string id = JsonStringField(entry, "id");
        if (id.empty()) continue;
        const std::wstring wideId(id.begin(), id.end());
        if (seen.insert(wideId).second) versions.push_back(wideId);
    }
    return versions;
}

std::vector<std::wstring> MinecraftLauncher::GetAvailableMinecraftVersions(const std::wstring& storageDir) {
    // Keep this cache separate from PrepareMinecraftFiles' launch manifest so
    // background refreshes can never race a game download or launch.
    const std::wstring manifestPath = storageDir + L"\\available_versions_manifest.json";
    const std::wstring refreshPath = manifestPath + L".refresh";
    auto readUtf8File = [](const std::wstring& filePath) -> std::string {
        FILE* file = _wfopen(filePath.c_str(), L"rb");
        if (!file) return "";
        fseek(file, 0, SEEK_END);
        const long size = ftell(file);
        fseek(file, 0, SEEK_SET);
        std::string contents;
        if (size > 0 && size < 20000000) {
            std::vector<char> data((size_t)size + 1, 0);
            const size_t read = fread(data.data(), 1, (size_t)size, file);
            contents.assign(data.data(), read);
        }
        fclose(file);
        return contents;
    };

    std::string manifestJson;
    const std::vector<std::wstring> manifestUrls = {
        L"https://piston-meta.mojang.com/mc/game/version_manifest_v2.json",
        L"https://launchermeta.mojang.com/mc/game/version_manifest_v2.json"
    };
    for (const std::wstring& url : manifestUrls) {
        DeleteFileW(refreshPath.c_str());
        if (!DownloadHttpFile(url, refreshPath, nullptr)) continue;
        std::string downloaded = readUtf8File(refreshPath);
        if (ParseReleaseVersionIds(downloaded).empty()) {
            DeleteFileW(refreshPath.c_str());
            continue;
        }
        MoveFileExW(refreshPath.c_str(), manifestPath.c_str(), MOVEFILE_REPLACE_EXISTING);
        manifestJson = std::move(downloaded);
        break;
    }
    DeleteFileW(refreshPath.c_str());

    if (manifestJson.empty()) manifestJson = readUtf8File(manifestPath);
    return ParseReleaseVersionIds(manifestJson);
}

static bool FileMatchesSha1(const std::wstring& path, const std::wstring& expectedHex) {
    if (expectedHex.size() != 40 || !CheckFileExists(path)) return false;
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    HANDLE file = INVALID_HANDLE_VALUE;
    bool valid = false;
    BYTE buffer[32768] = {};
    DWORD read = 0;
    bool readSucceeded = true;
    if (!CryptAcquireContextW(&provider, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) return false;
    if (!CryptCreateHash(provider, CALG_SHA1, 0, 0, &hash)) goto done;
    file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) goto done;
    while (true) {
        read = 0;
        if (!ReadFile(file, buffer, sizeof(buffer), &read, NULL)) {
            readSucceeded = false;
            break;
        }
        if (read == 0) break;
        if (!CryptHashData(hash, buffer, read, 0)) goto done;
    }
    if (!readSucceeded) goto done;
    {
        BYTE digest[20] = {};
        DWORD digestLength = sizeof(digest);
        if (!CryptGetHashParam(hash, HP_HASHVAL, digest, &digestLength, 0) || digestLength != sizeof(digest)) goto done;
        wchar_t actual[41] = {};
        for (DWORD i = 0; i < digestLength; ++i) swprintf(actual + i * 2, 3, L"%02x", digest[i]);
        valid = _wcsicmp(actual, expectedHex.c_str()) == 0;
    }
 done:
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    if (hash) CryptDestroyHash(hash);
    if (provider) CryptReleaseContext(provider, 0);
    return valid;
}

static bool IsValidLibraryArtifact(const VersionLibInfo& library, const std::wstring& path,
                                   std::wstring* failureReason = nullptr) {
    if (!CheckFileExists(path)) {
        if (failureReason) *failureReason = L"missing or empty file";
        return false;
    }
    if (!IsZipArchive(path)) {
        if (failureReason) *failureReason = L"invalid ZIP/JAR signature";
        return false;
    }
    if (!library.sha1.empty() && !FileMatchesSha1(path, library.sha1)) {
        if (failureReason) *failureReason = L"SHA-1 mismatch; expected " + library.sha1;
        return false;
    }
    if (failureReason) failureReason->clear();
    return true;
}

static bool PrepareLibraryRepairDestination(
    const std::wstring& libraryDir,
    const std::wstring& mcVersion,
    const std::wstring& loader,
    const VersionLibInfo& library,
    std::wstring& targetPath,
    std::function<void(const std::wstring&)> onDiagnostic,
    bool* recoveredCopyAlreadyValid,
    std::wstring* failureReason
) {
    if (recoveredCopyAlreadyValid) *recoveredCopyAlreadyValid = false;
    if (failureReason) failureReason->clear();
    const std::wstring originalPath = targetPath;
    const std::wstring recoveredPath = GetRecoveredLibraryPath(libraryDir, mcVersion, loader, library.path);
    std::wstring recoveredInvalidReason;
    if (IsValidLibraryArtifact(library, recoveredPath, &recoveredInvalidReason)) {
        targetPath = recoveredPath;
        if (recoveredCopyAlreadyValid) *recoveredCopyAlreadyValid = true;
        if (onDiagnostic) {
            onDiagnostic(L"[Prepare] Reusing verified recovery library instead of the invalid canonical copy: " +
                originalPath + L" -> " + recoveredPath);
        }
        return true;
    }
    DWORD originalError = ERROR_SUCCESS;
    bool originalWasReadOnly = false;
    const wchar_t* originalStage = L"";
    DWORD originalAttributes = INVALID_FILE_ATTRIBUTES;
    bool originalRemoved = false;
    for (int attempt = 0; attempt < 4; ++attempt) {
        if (RemoveExistingFileForReplacement(targetPath, &originalError, &originalWasReadOnly,
                                             &originalStage, &originalAttributes)) {
            originalRemoved = true;
            break;
        }
        if ((originalError != ERROR_ACCESS_DENIED && originalError != ERROR_SHARING_VIOLATION) || attempt == 3 ||
            wcscmp(originalStage, L"destination is a directory") == 0) break;
        Sleep(75 * (attempt + 1));
    }
    if (originalRemoved) {
        if (originalWasReadOnly && onDiagnostic) {
            onDiagnostic(L"[Prepare] Cleared read-only attribute from stale library: " + targetPath);
        }
        return true;
    }

    if (onDiagnostic) {
        onDiagnostic(L"[Prepare][WARN] Could not replace invalid library at " + originalPath +
            L" (stage=" + originalStage + L"; Win32 " + std::to_wstring(originalError) + L": " +
            FormatWin32Error(originalError) + L"; attributes=" + std::to_wstring(originalAttributes) +
            L"); attempting a separate recovery destination: " + recoveredPath);
    }

    DWORD recoveredError = ERROR_SUCCESS;
    const wchar_t* recoveredStage = L"";
    DWORD recoveredAttributes = INVALID_FILE_ATTRIBUTES;
    if (!RemoveExistingFileForReplacement(recoveredPath, &recoveredError, nullptr,
                                          &recoveredStage, &recoveredAttributes)) {
        if (failureReason) {
            *failureReason = L"cannot replace library at " + originalPath + L" (stage=" + originalStage +
                L"; Win32 " + std::to_wstring(originalError) + L": " + FormatWin32Error(originalError) +
                L"; attributes=" + std::to_wstring(originalAttributes) + L") and recovery cache is unavailable at " +
                recoveredPath + L" (stage=" + recoveredStage + L"; Win32 " +
                std::to_wstring(recoveredError) + L": " + FormatWin32Error(recoveredError) +
                L"; attributes=" + std::to_wstring(recoveredAttributes) + L")";
        }
        return false;
    }

    targetPath = recoveredPath;
    if (onDiagnostic) {
        onDiagnostic(L"[Prepare][WARN] Could not replace invalid library at " + originalPath +
            L" (stage=" + originalStage + L"; Win32 " + std::to_wstring(originalError) +
            L"; attributes=" + std::to_wstring(originalAttributes) +
            L"); storing a verified replacement separately at " + recoveredPath);
    }
    return true;
}

// A few networks expose Modrinth but block Fabric's Maven CDN. These three
// Fabric 0.16.10 files are immutable and independently SHA-1 verified against
// Fabric's public profile before they are accepted from a GitHub/jsDelivr mirror.
static bool DownloadVerifiedFabricMirror(
    const VersionLibInfo& library,
    const std::wstring& targetPath,
    std::function<void(const std::wstring&)> onDiagnostic
) {
    if (library.sha1.empty()) return false;
    std::wstring relative = library.path;
    for (wchar_t& c : relative) if (c == L'\\') c = L'/';
    const bool supported =
        relative == L"net/fabricmc/intermediary/1.20.1/intermediary-1.20.1.jar" ||
        relative == L"net/fabricmc/fabric-loader/0.16.10/fabric-loader-0.16.10.jar" ||
        relative == L"net/fabricmc/sponge-mixin/0.15.4+mixin.0.8.7/sponge-mixin-0.15.4+mixin.0.8.7.jar";
    if (!supported) return false;

    const std::vector<std::wstring> bases = {
        L"https://media.githubusercontent.com/media/NoaKajou/Distrib-launcher/main/repo/lib/",
        L"https://cdn.jsdelivr.net/gh/NoaKajou/Distrib-launcher@main/repo/lib/",
        L"https://raw.githubusercontent.com/NoaKajou/Distrib-launcher/main/repo/lib/"
    };
    for (const std::wstring& base : bases) {
        if (onDiagnostic) onDiagnostic(L"[Download] Trying preverified Fabric mirror: " + base + relative);
        if (MinecraftLauncher::DownloadHttpFile(base + relative, targetPath, nullptr, onDiagnostic) &&
            FileMatchesSha1(targetPath, library.sha1)) {
            if (onDiagnostic) onDiagnostic(L"[Download] Fabric mirror SHA-1 matched for " + relative);
            return true;
        }
        if (onDiagnostic) onDiagnostic(L"[Download] Fabric mirror failed or SHA-1 did not match for " + relative);
    }
    return false;
}

bool MinecraftLauncher::PrepareMinecraftFiles(
    const std::wstring& storageDir,
    const std::wstring& mcVersion,
    const std::wstring& loader,
    std::function<void(const std::wstring& status, int percent)> progress,
    std::function<void(const std::wstring& diagnostic)> onDiagnostic
) {
    std::wstring versionDir = storageDir + L"\\versions\\" + mcVersion;
    std::wstring versionJar = versionDir + L"\\" + mcVersion + L".jar";
    std::wstring versionJson = versionDir + L"\\" + mcVersion + L".json";
    EnsureDirTree(versionDir);

    std::wstring libDir = storageDir + L"\\libraries";
    EnsureDirTree(libDir);

    auto logDiagnostic = [&onDiagnostic](const std::wstring& message) {
        if (onDiagnostic) onDiagnostic(L"[Prepare] " + message);
    };
    logDiagnostic(L"Begin: Minecraft=" + mcVersion + L"; loader=" + loader +
                  L"; storage=" + storageDir + L"; version metadata=" + versionJson);

    // Keep the UI log actionable and emit a durable, stage-specific message
    // before returning so a failed preparation can be diagnosed after restart.
    auto reportPreparationFailure = [&progress, &logDiagnostic](const std::wstring& stage) {
        logDiagnostic(L"[ERROR] Preparation stopped: " + stage);
        if (progress) progress(L"Ошибка подготовки: " + stage, 0);
        return false;
    };

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
    if (HasUsableVersionMetadata(versionJson)) {
        logDiagnostic(L"Using existing version metadata: " + versionJson);
    } else {
        logDiagnostic(L"Local version metadata missing or invalid; checking official launcher cache.");
        DeleteFileW(versionJson.c_str());
        if (!appVersionDir.empty()) {
            const std::wstring sourceJson = appVersionDir + L"\\" + mcVersion + L".json";
            if (HasUsableVersionMetadata(sourceJson)) {
                EnsureParentDirectory(versionJson);
                if (CopyFileW(sourceJson.c_str(), versionJson.c_str(), FALSE)) {
                    logDiagnostic(L"Copied valid version metadata from: " + sourceJson);
                } else {
                    logDiagnostic(L"Could not copy cached version metadata (Win32 " +
                                  std::to_wstring(GetLastError()) + L"): " + sourceJson);
                }
            } else {
                logDiagnostic(L"Official launcher metadata not found or unusable: " + sourceJson);
            }
        } else {
            logDiagnostic(L"APPDATA is unavailable; no official .minecraft cache path can be checked.");
        }
    }

    if (!HasUsableVersionMetadata(versionJson)) {
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
        if (manifestJson.empty()) {
            logDiagnostic(L"Version manifest cache missing/unreadable; fetching official manifest.");
        } else {
            logDiagnostic(L"Read cached version manifest (" + std::to_wstring(manifestJson.size()) + L" bytes).");
        }
        if (GetVersionMetadataUrl(manifestJson, mcVersion).empty()) {
            logDiagnostic(L"Cached manifest did not contain Minecraft " + mcVersion + L"; fetching " + manifestUrl);
            DeleteFileW(manifestFile.c_str());
            if (MinecraftLauncher::DownloadHttpFile(manifestUrl, manifestFile, nullptr, onDiagnostic)) {
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
        if (metadataUrl.empty()) {
            return reportPreparationFailure(L"manifest contains no metadata URL for Minecraft " + mcVersion);
        }
        logDiagnostic(L"Downloading version metadata from: " + SanitizeDownloadUrl(metadataUrl));
        if (!MinecraftLauncher::DownloadHttpFile(metadataUrl, versionJson, nullptr, onDiagnostic)) {
            DeleteFileW(versionJson.c_str());
            return reportPreparationFailure(L"failed to download version metadata from " + SanitizeDownloadUrl(metadataUrl));
        }
        if (!HasUsableVersionMetadata(versionJson)) {
            DeleteFileW(versionJson.c_str());
            return reportPreparationFailure(L"downloaded version metadata is empty or has no client download URL: " + versionJson);
        }
        logDiagnostic(L"Version metadata downloaded and validated: " + versionJson);
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
    logDiagnostic(L"Version metadata read: " + std::to_wstring(versionJsonContent.size()) + L" bytes.");
    const std::wstring clientJarUrl = GetClientDownloadUrl(versionJsonContent);
    if (clientJarUrl.empty()) {
        // A non-empty file is not enough: cached HTML/partial metadata must not
        // cause the invalid old '/1.20.1/client.jar' fallback to be used.
        DeleteFileW(versionJson.c_str());
        return reportPreparationFailure(L"метаданные версии не содержат URL client.jar");
    }

    logDiagnostic(L"Client download URL: " + SanitizeDownloadUrl(clientJarUrl));

    // Step 2: Reuse or fetch an actual ZIP/JAR client. A failed CDN response is
    // not retained as a fake client.jar and the official launcher mirror is tried.
    if (IsZipArchive(versionJar)) {
        logDiagnostic(L"Using existing client JAR with ZIP signature: " + versionJar);
    } else if (!appVersionDir.empty()) {
        const std::wstring sourceJar = appVersionDir + L"\\" + mcVersion + L".jar";
        if (IsZipArchive(sourceJar)) {
            DeleteFileW(versionJar.c_str());
            EnsureParentDirectory(versionJar);
            if (CopyFileW(sourceJar.c_str(), versionJar.c_str(), FALSE)) {
                logDiagnostic(L"Reused client JAR from official launcher cache: " + sourceJar);
            } else {
                logDiagnostic(L"Could not copy official launcher client JAR (Win32 " +
                              std::to_wstring(GetLastError()) + L"): " + sourceJar);
            }
        } else {
            logDiagnostic(L"Official launcher client JAR missing or invalid: " + sourceJar);
        }
    } else {
        logDiagnostic(L"No APPDATA .minecraft directory; official launcher client cache unavailable.");
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
        }, onDiagnostic);
        if (!downloaded || !IsZipArchive(versionJar)) {
            return reportPreparationFailure(L"не удалось загрузить корректный официальный client.jar");
        }
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

    if (CheckFileExists(localIndexFile)) {
        logDiagnostic(L"Using existing asset index: " + localIndexFile);
    } else {
        bool foundInAppData = false;
        if (!mojangLibsDir.empty()) {
            WCHAR appDataAssets[MAX_PATH] = { 0 };
            if (GetEnvironmentVariableW(L"APPDATA", appDataAssets, MAX_PATH) > 0) {
                std::wstring appDataIndex = std::wstring(appDataAssets) + L"\\.minecraft\\assets\\indexes\\" + assetIndexId + L".json";
                if (CheckFileExists(appDataIndex)) {
                    foundInAppData = CopyFileW(appDataIndex.c_str(), localIndexFile.c_str(), FALSE) != FALSE;
                    logDiagnostic(foundInAppData ? L"Copied asset index from official launcher cache: " + appDataIndex
                                                  : L"Could not copy asset index (Win32 " + std::to_wstring(GetLastError()) + L"): " + appDataIndex);
                }
            }
        }
        if (!foundInAppData && !assetIndexUrl.empty()) {
            logDiagnostic(L"Asset index is missing; downloading from: " + SanitizeDownloadUrl(assetIndexUrl));
            foundInAppData = DownloadHttpFile(assetIndexUrl, localIndexFile, nullptr, onDiagnostic) &&
                             CheckFileExists(localIndexFile);
        }
        if (!foundInAppData) {
            logDiagnostic(L"[WARN] Asset index unavailable at " + localIndexFile +
                          L"; asset URL=" + SanitizeDownloadUrl(assetIndexUrl));
        }
    }

    // Step 2.6: If Fabric loader is selected, ensure Fabric libraries are present!
    if (loader == L"fabric") {
        if (progress) progress(L"Подготовка библиотек Fabric...", 50);
        // Exact Fabric 0.16.10 profile dependencies. The former hand-written
        // set used an obsolete sponge-mixin version and extra libraries, making
        // a successful Fabric Maven download still vulnerable to classpath drift.
        const std::vector<VersionLibInfo> fabricLibs = GetFabricLibraryArtifacts(mcVersion);

        for (const auto& flib : fabricLibs) {
            std::wstring targetPath = libDir + L"\\" + flib.path;
            std::wstring invalidReason;
            if (IsValidLibraryArtifact(flib, targetPath, &invalidReason)) {
                logDiagnostic(L"Fabric dependency already verified: " + targetPath);
                continue;
            }
            logDiagnostic(L"Fabric dependency needs repair: " + targetPath + L" (" + invalidReason + L")");
            bool recoveredCopyAlreadyValid = false;
            std::wstring destinationError;
            if (!PrepareLibraryRepairDestination(libDir, mcVersion, loader, flib, targetPath,
                    onDiagnostic, &recoveredCopyAlreadyValid, &destinationError)) {
                return reportPreparationFailure(destinationError);
            }
            if (recoveredCopyAlreadyValid) continue;
            bool found = false;

            if (!mojangLibsDir.empty()) {
                const std::wstring appDataLib = mojangLibsDir + L"\\" + flib.path;
                std::wstring sourceReason;
                if (IsValidLibraryArtifact(flib, appDataLib, &sourceReason)) {
                    EnsureParentDirectory(targetPath);
                    const BOOL copied = CopyFileW(appDataLib.c_str(), targetPath.c_str(), FALSE);
                    const DWORD copyError = copied ? ERROR_SUCCESS : GetLastError();
                    if (!copied) {
                        logDiagnostic(L"[ERROR] CopyFileW for cached Fabric dependency failed (Win32 " +
                            std::to_wstring(copyError) + L": " + FormatWin32Error(copyError) +
                            L"); source=" + appDataLib + L"; destination=" + targetPath);
                    } else if (IsValidLibraryArtifact(flib, targetPath, &invalidReason)) {
                        found = true;
                        logDiagnostic(L"Copied verified Fabric dependency from official cache: " + appDataLib);
                    } else {
                        logDiagnostic(L"Copied cached Fabric dependency failed validation: " + targetPath +
                                      L" (" + invalidReason + L")");
                    }
                } else if (CheckFileExists(appDataLib)) {
                    logDiagnostic(L"Official cache Fabric dependency rejected: " + appDataLib +
                                  L" (" + sourceReason + L")");
                }
            }

            if (!found && !flib.url.empty()) {
                logDiagnostic(L"Requesting Fabric dependency from its canonical Maven URL: " + SanitizeDownloadUrl(flib.url));
                const bool downloaded = DownloadHttpFile(flib.url, targetPath, nullptr, onDiagnostic);
                if (downloaded && IsValidLibraryArtifact(flib, targetPath, &invalidReason)) {
                    found = true;
                    logDiagnostic(L"Downloaded and verified Fabric dependency: " + targetPath);
                } else if (downloaded) {
                    logDiagnostic(L"Fabric dependency downloaded but rejected: " + targetPath +
                                  L" (" + invalidReason + L")");
                }
            }
            if (!found) {
                found = DownloadVerifiedFabricMirror(flib, targetPath, onDiagnostic) &&
                        IsValidLibraryArtifact(flib, targetPath, &invalidReason);
                if (found) logDiagnostic(L"Recovered Fabric dependency through verified mirror: " + targetPath);
                else logDiagnostic(L"No verified Fabric mirror artifact available for " + flib.path);
            }
            if (!found) {
                DWORD cleanupError = ERROR_SUCCESS;
                if (!RemoveExistingFileForReplacement(targetPath, &cleanupError)) {
                    logDiagnostic(L"[WARN] Could not remove failed Fabric artifact (Win32 " +
                        std::to_wstring(cleanupError) + L": " + FormatWin32Error(cleanupError) + L"): " + targetPath);
                }
                return reportPreparationFailure(L"failed to download or validate Fabric library " +
                                                flib.path + L"; last validation: " + invalidReason);
            }
        }
    }

    // Step 3: Parse and ensure ALL Minecraft Libraries (e.g. joptsimple, guava, gson, lwjgl)
    std::vector<VersionLibInfo> libsToEnsure;
    if (!versionJsonContent.empty()) {
        libsToEnsure = ParseLibrariesFromJson(versionJsonContent);
    }
    if (libsToEnsure.empty()) {
        logDiagnostic(L"Could not parse library entries from version JSON; using built-in fallback list.");
        libsToEnsure = GetStandardFallbackLibraries();
    }
    const size_t parsedNativeArchives = (size_t)std::count_if(libsToEnsure.begin(), libsToEnsure.end(),
        [](const VersionLibInfo& library) { return library.isNativeArchive; });
    const size_t parsedWindowsX64Natives = (size_t)std::count_if(libsToEnsure.begin(), libsToEnsure.end(),
        [](const VersionLibInfo& library) { return IsWindowsNativeArchivePath(library.path); });
    logDiagnostic(L"Checking " + std::to_wstring(libsToEnsure.size()) + L" Minecraft library artifacts; native entries=" +
        std::to_wstring(parsedNativeArchives) + L"; Windows x64 native paths=" +
        std::to_wstring(parsedWindowsX64Natives) + L".");

    if (progress) progress(L"Проверка и синхронизация библиотек...", 60);

    int totalLibs = (int)libsToEnsure.size();
    for (int i = 0; i < totalLibs; ++i) {
        const auto& lib = libsToEnsure[i];
        std::wstring targetPath = libDir + L"\\" + lib.path;
        std::wstring invalidReason;

        if (IsValidLibraryArtifact(lib, targetPath, &invalidReason)) {
            logDiagnostic(L"Minecraft library verified: " + targetPath);
        } else {
            logDiagnostic(L"Minecraft library missing/corrupt: " + targetPath + L" (" + invalidReason + L")");
            bool recoveredCopyAlreadyValid = false;
            std::wstring destinationError;
            if (!PrepareLibraryRepairDestination(libDir, mcVersion, loader, lib, targetPath,
                    onDiagnostic, &recoveredCopyAlreadyValid, &destinationError)) {
                return reportPreparationFailure(destinationError);
            }
            if (recoveredCopyAlreadyValid) continue;
            bool foundLibrary = false;

            if (!mojangLibsDir.empty()) {
                const std::wstring appDataLibPath = mojangLibsDir + L"\\" + lib.path;
                std::wstring sourceReason;
                if (IsValidLibraryArtifact(lib, appDataLibPath, &sourceReason)) {
                    EnsureParentDirectory(targetPath);
                    const BOOL copied = CopyFileW(appDataLibPath.c_str(), targetPath.c_str(), FALSE);
                    const DWORD copyError = copied ? ERROR_SUCCESS : GetLastError();
                    if (!copied) {
                        logDiagnostic(L"[ERROR] CopyFileW for cached Minecraft library failed (Win32 " +
                            std::to_wstring(copyError) + L": " + FormatWin32Error(copyError) +
                            L"); source=" + appDataLibPath + L"; destination=" + targetPath);
                    } else if (IsValidLibraryArtifact(lib, targetPath, &invalidReason)) {
                        foundLibrary = true;
                        logDiagnostic(L"Copied verified Minecraft library from official cache: " + appDataLibPath);
                    } else {
                        logDiagnostic(L"Copied cached Minecraft library failed validation: " + targetPath +
                                      L" (" + invalidReason + L")");
                    }
                } else if (CheckFileExists(appDataLibPath)) {
                    logDiagnostic(L"Official cache Minecraft library rejected: " + appDataLibPath +
                                  L" (" + sourceReason + L")");
                }
            }

            if (!foundLibrary && !lib.url.empty()) {
                logDiagnostic(L"Requesting Minecraft library from metadata URL: " + SanitizeDownloadUrl(lib.url));
                const bool downloaded = DownloadHttpFile(lib.url, targetPath, nullptr, onDiagnostic);
                if (downloaded && IsValidLibraryArtifact(lib, targetPath, &invalidReason)) {
                    foundLibrary = true;
                    logDiagnostic(L"Downloaded and verified Minecraft library: " + targetPath);
                } else if (downloaded) {
                    logDiagnostic(L"Downloaded Minecraft library failed artifact validation: " + targetPath +
                                  L" (" + invalidReason + L")");
                }
            }
            if (!foundLibrary && !lib.sha1.empty()) {
                const size_t slash = lib.path.find_last_of(L"\\/");
                const std::wstring name = slash == std::wstring::npos ? lib.path : lib.path.substr(slash + 1);
                const std::wstring pistonUrl = L"https://piston-data.mojang.com/v1/objects/" + lib.sha1 + L"/" + name;
                logDiagnostic(L"Retrying library from Mojang Piston Data: " + SanitizeDownloadUrl(pistonUrl));
                const bool downloaded = DownloadHttpFile(pistonUrl, targetPath, nullptr, onDiagnostic);
                if (downloaded && IsValidLibraryArtifact(lib, targetPath, &invalidReason)) {
                    foundLibrary = true;
                    logDiagnostic(L"Piston Data library verified: " + targetPath);
                } else if (downloaded) {
                    logDiagnostic(L"Piston Data copy failed artifact validation: " + targetPath +
                                  L" (" + invalidReason + L")");
                }
            }
            if (!foundLibrary) {
                DWORD cleanupError = ERROR_SUCCESS;
                if (!RemoveExistingFileForReplacement(targetPath, &cleanupError)) {
                    logDiagnostic(L"[WARN] Could not remove failed Minecraft artifact (Win32 " +
                        std::to_wstring(cleanupError) + L": " + FormatWin32Error(cleanupError) + L"): " + targetPath);
                }
                return reportPreparationFailure(L"failed to download or validate Minecraft library " +
                                                lib.path + L"; last validation: " + invalidReason);
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
    const MinecraftAccount& account,
    int memoryMb,
    const std::wstring& customJava,
    const std::wstring& jvmPreset,
    const std::wstring& customArgs,
    LaunchProgressCallback callbacks,
    HANDLE* outProcessHandle,
    const std::wstring& instanceDirOverride
) {
    auto logDiagnostic = [&callbacks](const std::wstring& message) {
        if (!callbacks.onLog) return;
        const bool isError = message.find(L"[ERROR]") != std::wstring::npos ||
                             message.find(L"[FATAL]") != std::wstring::npos;
        callbacks.onLog(message, isError);
    };
    auto reportLaunchFailure = [&callbacks](const std::wstring& reason, const std::wstring& status) {
        if (callbacks.onLog) callbacks.onLog(L"[Launch][ERROR] " + reason, true);
        if (callbacks.onProgress) callbacks.onProgress(status, 0);
        return false;
    };

    logDiagnostic(L"[Launch] Begin: Minecraft=" + mcVersion + L"; loader=" + loader +
                  L"; storage=" + storageDir + L"; memory=" + std::to_wstring(memoryMb) + L" MiB" +
                  L"; account=" + (account.isMicrosoft ? L"Microsoft" : L"offline") +
                  L"; Java override=" + (customJava.empty() ? L"(auto-detect)" : customJava));
    if (callbacks.onProgress) callbacks.onProgress(L"Проверка среды Java...", 5);
    logDiagnostic(L"[Launch] Resolving and validating Java before preparing game files.");

    const int requiredJava = GetRequiredJavaVersion(mcVersion);
    logDiagnostic(L"[Java] Minecraft " + mcVersion + L" requires Java " + std::to_wstring(requiredJava) + L"+.");
    JavaInfo java = DetectJava(customJava, requiredJava,
        [&callbacks](const std::wstring& message) {
            if (!callbacks.onLog) return;
            const bool isError = message.find(L"[ERROR]") != std::wstring::npos;
            callbacks.onLog(message, isError);
        });
    if (java.path.empty() || java.majorVersion <= 0) {
        return reportLaunchFailure(L"No runnable Java installation was detected. Set JAVA_HOME, add java.exe to PATH, or choose java.exe in Settings.",
                                   L"Java not found");
    }
    logDiagnostic(L"[Java] Selected runtime: Java " + std::to_wstring(java.majorVersion) +
                  L"; vendor=" + java.vendor + L"; architecture=" +
                  (java.architectureKnown ? (java.is64Bit ? L"x64" : L"x86") : L"unknown") +
                  L"; path=" + java.path);
    if (java.majorVersion < requiredJava) {
        return reportLaunchFailure(L"Minecraft " + mcVersion + L" requires Java " +
            std::to_wstring(requiredJava) + L" or newer, but Java " + std::to_wstring(java.majorVersion) +
            L" was selected at " + java.path + L".", L"Incompatible Java version");
    }
    if (java.architectureKnown && !java.is64Bit) {
        return reportLaunchFailure(L"The selected Java runtime is 32-bit; this x64 launcher requires a 64-bit Java runtime: " +
                                   java.path, L"Incompatible Java architecture");
    }
    if (account.isMicrosoft && (account.username.empty() || account.uuid.empty() || account.accessToken.empty())) {
        return reportLaunchFailure(L"Microsoft session is incomplete (missing profile name, UUID, or Minecraft access token); token values are intentionally not written to logs.",
                                   L"Microsoft sign-in required");
    }
    logDiagnostic(L"[Account] Session mode=" + std::wstring(account.isMicrosoft ? L"Microsoft" : L"offline") +
                  L"; profile present=" + (account.username.empty() ? L"no" : L"yes") +
                  L"; UUID present=" + (account.uuid.empty() ? L"no" : L"yes") +
                  L"; access token present=" + (account.accessToken.empty() ? L"no" : L"yes") +
                  L"; account values omitted from log.");

    MEMORYSTATUSEX memoryStatus = {};
    memoryStatus.dwLength = sizeof(memoryStatus);
    if (GlobalMemoryStatusEx(&memoryStatus)) {
        const ULONGLONG availableMiB = memoryStatus.ullAvailPhys / (1024ULL * 1024ULL);
        const ULONGLONG totalMiB = memoryStatus.ullTotalPhys / (1024ULL * 1024ULL);
        logDiagnostic(L"[Memory] Available physical RAM=" + std::to_wstring(availableMiB) +
                      L" MiB; total=" + std::to_wstring(totalMiB) + L" MiB; configured Xmx=" +
                      std::to_wstring(memoryMb) + L" MiB.");
        if (memoryMb > 0 && static_cast<ULONGLONG>(memoryMb) > availableMiB) {
            logDiagnostic(L"[Memory][WARN] Configured heap exceeds currently available physical RAM; JVM startup may fail or paging may be severe.");
        }
    } else {
        logDiagnostic(L"[Memory][WARN] GlobalMemoryStatusEx failed (Win32 " +
                      std::to_wstring(GetLastError()) + L").");
    }

    // 1. Prepare game files once. All downloader and validation diagnostics are
    // forwarded to the durable launcher log before a failure is returned.
    if (callbacks.onProgress) callbacks.onProgress(L"Подготовка файлов игры и библиотек...", 15);
    const bool filesOk = PrepareMinecraftFiles(storageDir, mcVersion, loader, callbacks.onProgress,
        [&callbacks](const std::wstring& message) {
            if (callbacks.onLog) {
                const bool isError = message.find(L"[ERROR]") != std::wstring::npos;
                callbacks.onLog(message, isError);
            }
        });
    if (!filesOk) {
        return reportLaunchFailure(L"Game-file preparation failed for Minecraft " + mcVersion +
                                   L"; review the preceding [Prepare] and [Download] diagnostics.",
                                   L"Game file preparation failed");
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
    logDiagnostic(L"[Paths] Client JAR=" + clientJar + L"; instance=" + instanceDir +
                  L"; assets=" + assetsDir + L"; natives=" + nativesDir);

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
    const size_t metadataNativeArchives = (size_t)std::count_if(metadataLibraries.begin(), metadataLibraries.end(),
        [](const VersionLibInfo& library) { return library.isNativeArchive; });
    const size_t metadataWindowsX64Natives = (size_t)std::count_if(metadataLibraries.begin(), metadataLibraries.end(),
        [](const VersionLibInfo& library) { return IsWindowsNativeArchivePath(library.path); });
    const std::wstring localLibrariesDir = storageDir + L"\\libraries";
    logDiagnostic(L"[Metadata] Version JSON bytes=" + std::to_wstring(versionMetadata.size()) +
                  L"; parsed libraries=" + std::to_wstring(metadataLibraries.size()) +
                  L"; native entries=" + std::to_wstring(metadataNativeArchives) +
                  L"; Windows x64 native paths=" + std::to_wstring(metadataWindowsX64Natives) + L".");
    if (metadataLibraries.empty()) {
        logDiagnostic(L"[Natives][ERROR] Version metadata has no parseable library entries.");
        if (callbacks.onProgress) callbacks.onProgress(L"Ошибка подготовки LWJGL natives", 0);
        return false;
    }
    const bool nativesReady = ExtractWindowsNatives(metadataLibraries, localLibrariesDir, nativesDir,
        mcVersion, loader, [&callbacks](const std::wstring& message) {
            if (callbacks.onLog) callbacks.onLog(message,
                message.find(L"[ERROR]") != std::wstring::npos);
        });
    if (!nativesReady) {
        logDiagnostic(L"[Natives][ERROR] Native extraction failed; Java will not be started.");
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
    logDiagnostic(L"[Assets] Effective asset directory=" + effectiveAssetsDir);

    // 4. Assemble the complete Java classpath from current-version dependencies.
    std::vector<std::wstring> allJars;
    const std::wstring libDir = storageDir + L"\\libraries";

    // Prioritize current-version artifacts only after validating them against
    // Mojang or pinned Fabric hashes. If an inaccessible corrupt file blocks
    // replacement, its verified per-version recovery copy wins de-duplication.
    std::vector<VersionLibInfo> launchLibraries = metadataLibraries;
    if (loader == L"fabric") {
        const std::vector<VersionLibInfo> fabricLibraries = GetFabricLibraryArtifacts(mcVersion);
        launchLibraries.insert(launchLibraries.end(), fabricLibraries.begin(), fabricLibraries.end());
    }
    for (const VersionLibInfo& library : launchLibraries) {
        const std::wstring canonicalPath = libDir + L"\\" + library.path;
        std::wstring invalidReason;
        if (IsValidLibraryArtifact(library, canonicalPath, &invalidReason)) {
            allJars.push_back(canonicalPath);
            continue;
        }
        const std::wstring recoveredPath = GetRecoveredLibraryPath(libDir, mcVersion, loader, library.path);
        if (IsValidLibraryArtifact(library, recoveredPath, &invalidReason)) {
            allJars.push_back(recoveredPath);
            logDiagnostic(L"[Classpath] Prioritizing verified recovery library: " + recoveredPath);
        }
    }

    // Vanilla and Fabric have complete explicit dependency metadata above;
    // do not sweep old versions from the shared cache into their classpaths.
    // Other loaders may have additional libraries without Mojang metadata, so
    // retain the legacy scan for those profiles only. Recovery JARs are never
    // recursively trusted; each one must match its current metadata entry.
    if (loader != L"vanilla" && loader != L"fabric") {
        std::vector<std::wstring> cachedJars;
        CollectJarsRecursively(libDir, cachedJars);
        allJars.insert(allJars.end(), cachedJars.begin(), cachedJars.end());
    }
    logDiagnostic(L"[Classpath] Prepared " + std::to_wstring(allJars.size()) +
                  L" library candidates" +
                  ((loader == L"vanilla" || loader == L"fabric")
                      ? L" from current metadata; shared-cache stale artifacts excluded."
                      : L" including loader-cache candidates under " + libDir + L"."));

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
        if (lowerName.find(L"-natives-") != std::wstring::npos) {
            logDiagnostic(L"[Classpath] Excluding native-only archive: " + jarPath);
            continue;
        }
        if ((loader == L"fabric" || loader == L"quilt") &&
            lowerName.find(L"fabric-loader-") == 0 && lowerName != L"fabric-loader-0.16.10.jar") {
            logDiagnostic(L"[Classpath] Excluding obsolete Fabric loader: " + jarPath);
            continue;
        }
        if ((loader == L"fabric" || loader == L"quilt") &&
            lowerName.find(L"sponge-mixin-") == 0 &&
            lowerName != L"sponge-mixin-0.15.4+mixin.0.8.7.jar") {
            logDiagnostic(L"[Classpath] Excluding obsolete Mixin runtime: " + jarPath);
            continue;
        }

        std::wstring artKey = GetArtifactKey(jarPath);

        // Deduplicate both by exact jar filename and by maven artifact ID (e.g. prevent duplicate fabric-loader or mixin)!
        if (seenJarNames.find(lowerName) == seenJarNames.end() &&
            (artKey.empty() || seenArtifacts.find(artKey) == seenArtifacts.end())) {
            seenJarNames.insert(lowerName);
            if (!artKey.empty()) seenArtifacts.insert(artKey);
            classpath += L";\"" + jarPath + L"\"";
            ++addedCount;
            logDiagnostic(L"[Classpath] Added JAR: " + jarPath +
                          (artKey.empty() ? L"" : L"; artifact=" + artKey));
        } else {
            logDiagnostic(L"[Classpath] Skipped duplicate JAR: " + jarPath +
                          (artKey.empty() ? L"" : L"; artifact=" + artKey));
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
    logDiagnostic(L"[Launch] Resolved main class=" + mainClass + L"; asset index=" + assetIndexId +
                  L"; classpath length=" + std::to_wstring(classpath.size()) +
                  L"; Java library path=" + nativesDir);

    // 5. Build JVM and game arguments. Honor low custom RAM values instead of
    // silently forcing a 4 GiB heap, and never let Xms exceed Xmx.
    const int effectiveMemoryMb = std::max(512, memoryMb);
    const int initialMemoryMb = std::min(1024, effectiveMemoryMb);
    const std::wstring memArg = L"-Xmx" + std::to_wstring(effectiveMemoryMb) + L"M -Xms" +
                                std::to_wstring(initialMemoryMb) + L"M";
    logDiagnostic(L"[Memory] Effective JVM heap: Xmx=" + std::to_wstring(effectiveMemoryMb) +
                  L" MiB; Xms=" + std::to_wstring(initialMemoryMb) + L" MiB.");
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

    const std::wstring finalNickname = account.username.empty() ? L"Player" : account.username;
    const std::wstring finalUuid = account.isMicrosoft && !account.uuid.empty()
        ? account.uuid : L"00000000-0000-0000-0000-000000000000";
    const std::wstring finalAccessToken = account.isMicrosoft && !account.accessToken.empty()
        ? account.accessToken : L"0";
    const std::wstring userType = account.isMicrosoft ? L"msa" : L"legacy";
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
        L"--uuid \"" + finalUuid + L"\" " +
        L"--accessToken \"" + finalAccessToken + L"\" " +
        L"--userType " + userType + L" " +
        L"--versionType " + versionType;

    // Do not persist the full command line. Besides the Microsoft access token,
    // user-supplied JVM arguments and account identifiers can contain secrets or
    // personal data; a summary is enough to diagnose process creation failures.
    logDiagnostic(L"[Process] Java launch request prepared: executable=" + java.path +
                  L"; working directory=" + instanceDir +
                  L"; command-line characters=" + std::to_wstring(cmdLine.size()) +
                  L"; classpath JARs=" + std::to_wstring(addedCount) +
                  L"; main class=" + mainClass +
                  L"; account values and JVM arguments omitted from log.");

    std::vector<std::wstring> outputRedactionValues;
    AddSensitiveOutputValue(outputRedactionValues, account.username);
    AddSensitiveOutputValue(outputRedactionValues, account.uuid);
    AddSensitiveOutputValue(outputRedactionValues, finalAccessToken);
    AddSensitiveJvmArguments(customArgs, outputRedactionValues);

    if (cmdLine.size() >= 32767) {
        return reportLaunchFailure(L"Java command line exceeds the Windows CreateProcessW limit (32766 characters). Reduce the classpath or shorten the storage path.",
                                   L"Java command line is too long");
    }
    if (!CheckFileExists(java.path)) {
        return reportLaunchFailure(L"Selected Java executable disappeared or cannot be read: " + java.path,
                                   L"Java executable unavailable");
    }
    if (!CheckFileExists(clientJar)) {
        return reportLaunchFailure(L"Prepared Minecraft client JAR is missing or empty: " + clientJar,
                                   L"Client JAR missing");
    }
    const DWORD instanceAttributes = GetFileAttributesW(instanceDir.c_str());
    if (instanceAttributes == INVALID_FILE_ATTRIBUTES || !(instanceAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        const DWORD error = GetLastError();
        return reportLaunchFailure(L"Instance working directory does not exist: " + instanceDir +
            L" (Win32 " + std::to_wstring(error) + L": " + FormatWin32Error(error) + L").",
            L"Instance directory unavailable");
    }

    if (callbacks.onProgress) callbacks.onProgress(L"Запуск процесса игры...", 90);

    // 6. Spawn Java with stdout/stderr redirected into one captured UTF-8/ACP
    // stream. The access token is never written to diagnostics.
    SECURITY_ATTRIBUTES security = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE hStdOutRead = NULL;
    HANDLE hStdOutWrite = NULL;
    if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &security, 0)) {
        const DWORD error = GetLastError();
        return reportLaunchFailure(L"CreatePipe for Minecraft output failed (Win32 " +
            std::to_wstring(error) + L": " + FormatWin32Error(error) + L").", L"Could not capture game output");
    }
    if (!SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0)) {
        const DWORD error = GetLastError();
        CloseHandle(hStdOutRead);
        CloseHandle(hStdOutWrite);
        return reportLaunchFailure(L"SetHandleInformation for Minecraft output pipe failed (Win32 " +
            std::to_wstring(error) + L": " + FormatWin32Error(error) + L").", L"Could not capture game output");
    }

    HANDLE hStdIn = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hStdIn == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        CloseHandle(hStdOutRead);
        CloseHandle(hStdOutWrite);
        return reportLaunchFailure(L"Could not open NUL as Java stdin (Win32 " +
            std::to_wstring(error) + L": " + FormatWin32Error(error) + L").", L"Could not prepare Java process");
    }

    STARTUPINFOW startup = { sizeof(STARTUPINFOW) };
    startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    startup.hStdInput = hStdIn;
    startup.hStdOutput = hStdOutWrite;
    startup.hStdError = hStdOutWrite;

    PROCESS_INFORMATION process = {};
    std::vector<wchar_t> mutableCommandLine(cmdLine.begin(), cmdLine.end());
    mutableCommandLine.push_back(L'\0');
    if (outProcessHandle) *outProcessHandle = NULL;

    logDiagnostic(L"[Process] Calling CreateProcessW with explicit executable and instance working directory.");
    const BOOL processCreated = CreateProcessW(
        java.path.c_str(),
        mutableCommandLine.data(),
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW,
        NULL,
        instanceDir.c_str(),
        &startup,
        &process
    );
    const DWORD createError = processCreated ? ERROR_SUCCESS : GetLastError();
    CloseHandle(hStdOutWrite);
    CloseHandle(hStdIn);

    if (!processCreated) {
        CloseHandle(hStdOutRead);
        return reportLaunchFailure(L"CreateProcessW could not start Java (Win32 " +
            std::to_wstring(createError) + L": " + FormatWin32Error(createError) +
            L"); application=" + java.path + L"; working directory=" + instanceDir +
            L"; command-line characters=" + std::to_wstring(cmdLine.size()) + L".",
            L"Java process failed to start");
    }

    if (outProcessHandle) *outProcessHandle = process.hProcess;
    logDiagnostic(L"[Process] Java process created; PID=" + std::to_wstring(process.dwProcessId) +
                  L"; TID=" + std::to_wstring(process.dwThreadId) + L".");
    if (callbacks.onProgress) callbacks.onProgress(L"Игра запущена!", 100);

    // Drain the merged output pipe without dropping the last unterminated line.
    // The returned process handle belongs to the caller when outProcessHandle
    // is supplied; otherwise this monitor closes it after collecting the exit.
    const bool processHandleTransferred = outProcessHandle != nullptr;
    std::thread([hStdOutRead, process, callbacks, processHandleTransferred,
                 outputRedactionValues = std::move(outputRedactionValues)]() {
        auto emitLine = [&callbacks, &outputRedactionValues](const std::string& rawLine) {
            if (rawLine.empty() || !callbacks.onLog) return;
            const std::wstring line = RedactSensitiveProcessOutput(
                DecodeProcessOutput(rawLine), outputRedactionValues);
            std::wstring uppercase = line;
            std::transform(uppercase.begin(), uppercase.end(), uppercase.begin(), ::towupper);
            const bool isError = uppercase.find(L"ERROR") != std::wstring::npos ||
                                 uppercase.find(L"FATAL") != std::wstring::npos ||
                                 uppercase.find(L"EXCEPTION") != std::wstring::npos ||
                                 uppercase.find(L"OUTOFMEMORY") != std::wstring::npos;
            callbacks.onLog(L"[Minecraft] " + line, isError);
        };

        std::string accumulated;
        ULONGLONG totalOutputBytes = 0;
        DWORD outputReadError = ERROR_SUCCESS;
        char buffer[4096];
        DWORD bytesRead = 0;
        while (true) {
            if (!ReadFile(hStdOutRead, buffer, sizeof(buffer), &bytesRead, NULL)) {
                outputReadError = GetLastError();
                break;
            }
            if (bytesRead == 0) break;
            totalOutputBytes += bytesRead;
            accumulated.append(buffer, bytesRead);

            size_t newlinePosition = std::string::npos;
            while ((newlinePosition = accumulated.find_first_of("\r\n")) != std::string::npos) {
                const std::string line = accumulated.substr(0, newlinePosition);
                size_t consumed = newlinePosition + 1;
                if (accumulated[newlinePosition] == '\r' && consumed < accumulated.size() &&
                    accumulated[consumed] == '\n') ++consumed;
                accumulated.erase(0, consumed);
                emitLine(line);
            }
            while (accumulated.size() > 24000) {
                emitLine(accumulated.substr(0, 12000));
                accumulated.erase(0, 12000);
            }
        }
        if (!accumulated.empty()) emitLine(accumulated);
        CloseHandle(hStdOutRead);

        if (outputReadError != ERROR_SUCCESS && outputReadError != ERROR_BROKEN_PIPE && callbacks.onLog) {
            callbacks.onLog(L"[Process][ERROR] Reading Java output failed (Win32 " +
                std::to_wstring(outputReadError) + L": " + FormatWin32Error(outputReadError) + L").", true);
        }
        if (callbacks.onLog) {
            callbacks.onLog(L"[Process] Java output stream closed; captured " +
                std::to_wstring(totalOutputBytes) + L" bytes" +
                (outputReadError == ERROR_BROKEN_PIPE ? L" (broken pipe at child exit)." : L"."), false);
        }

        const DWORD waitResult = WaitForSingleObject(process.hProcess, INFINITE);
        const DWORD waitError = waitResult == WAIT_FAILED ? GetLastError() : ERROR_SUCCESS;
        DWORD exitCode = STILL_ACTIVE;
        const BOOL exitCodeRead = GetExitCodeProcess(process.hProcess, &exitCode);
        if (waitResult != WAIT_OBJECT_0 && callbacks.onLog) {
            callbacks.onLog(L"[Process][ERROR] Waiting for Java process failed (wait=" +
                std::to_wstring(waitResult) + L"; Win32=" + std::to_wstring(waitError) +
                L": " + FormatWin32Error(waitError) + L").", true);
        }
        if (!exitCodeRead && callbacks.onLog) {
            const DWORD error = GetLastError();
            callbacks.onLog(L"[Process][ERROR] GetExitCodeProcess failed (Win32 " +
                std::to_wstring(error) + L": " + FormatWin32Error(error) + L").", true);
        }
        if (callbacks.onLog) {
            callbacks.onLog(L"[Process] Java process completed; PID=" + std::to_wstring(process.dwProcessId) +
                L"; exit code=" + std::to_wstring(exitCode) +
                (exitCodeRead && exitCode == 0 ? L" (success)." : L" (failure/unknown)."),
                !exitCodeRead || exitCode != 0);
        }

        CloseHandle(process.hThread);
        if (callbacks.onGameExited) callbacks.onGameExited(static_cast<int>(exitCode));
        if (!processHandleTransferred) CloseHandle(process.hProcess);
    }).detach();

    return true;
}

bool MinecraftLauncher::TerminateRunningGame(HANDLE hProcess, DWORD* errorCode) {
    if (errorCode) *errorCode = ERROR_SUCCESS;
    if (!hProcess || hProcess == INVALID_HANDLE_VALUE) {
        if (errorCode) *errorCode = ERROR_INVALID_HANDLE;
        return false;
    }
    if (!TerminateProcess(hProcess, 0)) {
        const DWORD error = GetLastError();
        if (errorCode) *errorCode = error;
        return false;
    }
    return true;
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
