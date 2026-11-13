import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import https from 'node:https';
import http from 'node:http';
import { EventEmitter } from 'node:events';
import { getSystemCa } from './http_utils.js';
export class AutoUpdater extends EventEmitter {
    currentVersion;
    repoOwner;
    repoName;
    cachePath;
    constructor(currentVersion = '1.0.38', repoOwner = 'Sqikpeek', repoName = 'Aether', cacheDir) {
        super();
        this.currentVersion = currentVersion.replace(/^v/, '').trim();
        this.repoOwner = repoOwner;
        this.repoName = repoName;
        this.cachePath = path.join(cacheDir || process.cwd(), '.update_cache.json');
    }
    getCurrentVersion() {
        return this.currentVersion;
    }
    /**
     * Compares two semantic version strings (e.g. "1.0.38" vs "1.0.37").
     * Returns:
     *   1 if v1 > v2
     *  -1 if v1 < v2
     *   0 if v1 === v2
     */
    static compareVersions(v1, v2) {
        const clean1 = v1.replace(/^v/, '').trim();
        const clean2 = v2.replace(/^v/, '').trim();
        const parts1 = clean1.split('.').map(p => parseInt(p, 10) || 0);
        const parts2 = clean2.split('.').map(p => parseInt(p, 10) || 0);
        const maxLength = Math.max(parts1.length, parts2.length);
        for (let i = 0; i < maxLength; i++) {
            const p1 = parts1[i] || 0;
            const p2 = parts2[i] || 0;
            if (p1 > p2)
                return 1;
            if (p1 < p2)
                return -1;
        }
        return 0;
    }
    /**
     * Checks for updates using a multi-tier fallback mechanism:
     * 1. GitHub Releases API (/repos/{owner}/{repo}/releases/latest)
     * 2. GitHub Releases Redirect (/releases/latest HTTP 302 location)
     * 3. Repository Raw Manifest (update_manifest.json)
     * 4. Cached manifest if offline
     */
    async checkForUpdates() {
        const errors = [];
        // Strategy 1: GitHub Releases API
        try {
            const release = await this.fetchGitHubRelease();
            if (release && release.tag_name) {
                const latestVersion = release.tag_name.replace(/^v/, '').trim();
                const asset = this.findBestAsset(release.assets || []);
                const manifest = this.buildManifestFromRelease(release, asset);
                this.saveCachedManifest(manifest);
                const hasUpdate = AutoUpdater.compareVersions(latestVersion, this.currentVersion) > 0;
                return {
                    hasUpdate,
                    currentVersion: this.currentVersion,
                    latestVersion,
                    manifest,
                    source: 'github-api'
                };
            }
        }
        catch (err) {
            errors.push(`GitHub API: ${err.message}`);
        }
        // Strategy 2: GitHub Releases HTTP Redirect
        try {
            const redirectTag = await this.fetchLatestTagViaRedirect();
            if (redirectTag) {
                const latestVersion = redirectTag.replace(/^v/, '').trim();
                const assetUrl = `https://github.com/${this.repoOwner}/${this.repoName}/releases/download/${redirectTag}/Aether.zip`;
                const manifest = {
                    version: latestVersion,
                    title: `Aether ${latestVersion}`,
                    title_en: `Aether ${latestVersion}`,
                    title_ru: `Aether ${latestVersion}`,
                    url: assetUrl,
                    notes: 'Update detected via GitHub release tag',
                    notes_en: 'Update detected via GitHub release tag',
                    notes_ru: 'Обновление обнаружено через тег релиза GitHub',
                    changes: ['Update package from GitHub Releases'],
                    changes_en: ['Update package from GitHub Releases'],
                    changes_ru: ['Пакет обновления из GitHub Releases']
                };
                this.saveCachedManifest(manifest);
                const hasUpdate = AutoUpdater.compareVersions(latestVersion, this.currentVersion) > 0;
                return {
                    hasUpdate,
                    currentVersion: this.currentVersion,
                    latestVersion,
                    manifest,
                    source: 'github-redirect'
                };
            }
        }
        catch (err) {
            errors.push(`GitHub Redirect: ${err.message}`);
        }
        // Strategy 3: Raw update_manifest.json from repository
        try {
            const rawManifest = await this.fetchRawManifest();
            if (rawManifest && rawManifest.version) {
                const latestVersion = rawManifest.version.replace(/^v/, '').trim();
                this.saveCachedManifest(rawManifest);
                const hasUpdate = AutoUpdater.compareVersions(latestVersion, this.currentVersion) > 0;
                return {
                    hasUpdate,
                    currentVersion: this.currentVersion,
                    latestVersion,
                    manifest: rawManifest,
                    source: 'raw-manifest'
                };
            }
        }
        catch (err) {
            errors.push(`Raw Manifest: ${err.message}`);
        }
        // Strategy 4: Local cache
        const cached = this.loadCachedManifest();
        if (cached) {
            const latestVersion = cached.version.replace(/^v/, '').trim();
            const hasUpdate = AutoUpdater.compareVersions(latestVersion, this.currentVersion) > 0;
            return {
                hasUpdate,
                currentVersion: this.currentVersion,
                latestVersion,
                manifest: cached,
                source: 'cached',
                error: `Network unavailable. Used cached manifest. Errors: ${errors.join('; ')}`
            };
        }
        return {
            hasUpdate: false,
            currentVersion: this.currentVersion,
            latestVersion: this.currentVersion,
            manifest: null,
            error: `All update check strategies failed: ${errors.join('; ')}`
        };
    }
    /**
     * Downloads update archive with streaming SHA-256 calculation and progress reporting.
     */
    async downloadUpdate(downloadUrl, destinationPath, expectedSha256, expectedSize, onProgress) {
        const dir = path.dirname(destinationPath);
        if (!fs.existsSync(dir)) {
            fs.mkdirSync(dir, { recursive: true });
        }
        const tempPath = `${destinationPath}.tmp.${Date.now()}`;
        const fileStream = fs.createWriteStream(tempPath);
        const hash = crypto.createHash('sha256');
        return new Promise((resolve, reject) => {
            let bytesDownloaded = 0;
            let totalBytes = expectedSize || 0;
            let startTime = Date.now();
            let lastReportTime = startTime;
            const makeRequest = (targetUrl, redirectCount = 0) => {
                if (redirectCount > 10) {
                    fileStream.close();
                    fs.unlink(tempPath, () => { });
                    return reject(new Error('Too many redirects while downloading update'));
                }
                const client = targetUrl.startsWith('https:') ? https : http;
                const ca = targetUrl.startsWith('https:') ? getSystemCa() : undefined;
                const req = client.get(targetUrl, {
                    ca,
                    headers: {
                        'User-Agent': 'Aether-Launcher-Updater/1.0',
                        Accept: 'application/octet-stream, application/zip, */*'
                    }
                }, res => {
                    // Handle redirects (301, 302, 303, 307, 308)
                    if (res.statusCode && res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
                        const redirectUrl = new URL(res.headers.location, targetUrl).toString();
                        return makeRequest(redirectUrl, redirectCount + 1);
                    }
                    if (res.statusCode !== 200) {
                        fileStream.close();
                        fs.unlink(tempPath, () => { });
                        return reject(new Error(`Failed to download update: HTTP ${res.statusCode} ${res.statusMessage || ''}`));
                    }
                    const headerLen = res.headers['content-length'];
                    if (headerLen) {
                        totalBytes = parseInt(headerLen, 10);
                    }
                    res.on('data', chunk => {
                        bytesDownloaded += chunk.length;
                        hash.update(chunk);
                        fileStream.write(chunk);
                        const now = Date.now();
                        if (now - lastReportTime >= 100 || bytesDownloaded === totalBytes) {
                            lastReportTime = now;
                            const elapsedSec = (now - startTime) / 1000 || 0.001;
                            const speedBytesPerSec = bytesDownloaded / elapsedSec;
                            const remainingBytes = Math.max(0, totalBytes - bytesDownloaded);
                            const etaSeconds = speedBytesPerSec > 0 ? Math.round(remainingBytes / speedBytesPerSec) : 0;
                            const percent = totalBytes > 0 ? Math.min(100, Math.round((bytesDownloaded / totalBytes) * 100)) : 0;
                            const progressData = {
                                bytesDownloaded,
                                totalBytes,
                                percent,
                                speedBytesPerSec: Math.round(speedBytesPerSec),
                                etaSeconds
                            };
                            if (onProgress)
                                onProgress(progressData);
                            this.emit('progress', progressData);
                        }
                    });
                    res.on('end', () => {
                        fileStream.end(async () => {
                            const calculatedSha256 = hash.digest('hex').toLowerCase();
                            let verifiedSha256 = false;
                            if (expectedSha256) {
                                const expectedClean = expectedSha256.trim().toLowerCase();
                                if (expectedClean.length === 64) {
                                    if (expectedClean !== calculatedSha256) {
                                        fs.unlink(tempPath, () => { });
                                        return reject(new Error(`SHA-256 checksum mismatch! Expected: ${expectedClean}, Calculated: ${calculatedSha256}`));
                                    }
                                    verifiedSha256 = true;
                                }
                            }
                            // Atomic rename
                            try {
                                if (fs.existsSync(destinationPath)) {
                                    fs.unlinkSync(destinationPath);
                                }
                                fs.renameSync(tempPath, destinationPath);
                                resolve({
                                    path: destinationPath,
                                    verifiedSha256,
                                    calculatedSha256
                                });
                            }
                            catch (err) {
                                reject(err);
                            }
                        });
                    });
                    res.on('error', err => {
                        fileStream.close();
                        fs.unlink(tempPath, () => { });
                        reject(err);
                    });
                });
                req.on('error', err => {
                    fileStream.close();
                    fs.unlink(tempPath, () => { });
                    reject(err);
                });
                req.setTimeout(30000, () => {
                    req.destroy(new Error('Connection timed out while downloading update'));
                });
            };
            makeRequest(downloadUrl);
        });
    }
    /**
     * Generates a platform-specific updater script that safely waits for the launcher
     * process to exit, backs up old files, unpacks new files, cleans environment variables,
     * and restarts the launcher hidden without console flash.
     */
    generateUpdateScript(archivePath, installDir, targetExeName = 'Aether.exe', parentPid = process.pid) {
        const isWindows = process.platform === 'win32';
        const scriptExt = isWindows ? 'bat' : 'sh';
        const scriptPath = path.join(installDir, `apply_update.${scriptExt}`);
        if (isWindows) {
            const batContent = `@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

set "PID=${parentPid}"
set "ARCHIVE=${archivePath.replace(/\//g, '\\')}"
set "TARGET_DIR=${installDir.replace(/\//g, '\\')}"
set "TARGET_EXE=${targetExeName}"

:: Step 1: Wait for parent process to exit cleanly
:WAIT_LOOP
tasklist /FI "PID eq %PID%" 2>nul | find "%PID%" >nul
if not errorlevel 1 (
    timeout /T 1 /NOBREAK >nul
    goto WAIT_LOOP
)

:: Step 2: Create backup directory
if not exist "%TARGET_DIR%\\.backup" mkdir "%TARGET_DIR%\\.backup"
if exist "%TARGET_DIR%\\%TARGET_EXE%" copy /Y "%TARGET_DIR%\\%TARGET_EXE%" "%TARGET_DIR%\\.backup\\%TARGET_EXE%.old" >nul

:: Step 3: Extract archive safely
powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command "Expand-Archive -LiteralPath '%ARCHIVE%' -DestinationPath '%TARGET_DIR%' -Force"

:: Step 4: Clear PyInstaller environment variables to prevent dll load failure
set "_MEIPASS2="
set "_MEIPASS="
set "PYTHONPATH="
set "PYTHONHOME="

:: Step 5: Restart the updated launcher without visible cmd window
cd /d "%TARGET_DIR%"
start "" /B "%TARGET_EXE%"

:: Step 6: Self delete updater script
del "%~f0" >nul 2>&1
exit
`;
            fs.writeFileSync(scriptPath, batContent, 'utf-8');
            return {
                scriptPath,
                command: 'cmd.exe',
                args: ['/c', scriptPath]
            };
        }
        else {
            const shContent = `#!/bin/sh
PID="${parentPid}"
ARCHIVE="${archivePath}"
TARGET_DIR="${installDir}"
TARGET_EXE="${targetExeName}"

# Wait for parent PID to exit
while kill -0 "$PID" 2>/dev/null; do
    sleep 0.5
done

mkdir -p "$TARGET_DIR/.backup"
if [ -f "$TARGET_DIR/$TARGET_EXE" ]; then
    cp -f "$TARGET_DIR/$TARGET_EXE" "$TARGET_DIR/.backup/$TARGET_EXE.old"
fi

unzip -o "$ARCHIVE" -d "$TARGET_DIR"

chmod +x "$TARGET_DIR/$TARGET_EXE" 2>/dev/null || true

cd "$TARGET_DIR"
nohup "./$TARGET_EXE" >/dev/null 2>&1 &

rm -f "$0"
exit 0
`;
            fs.writeFileSync(scriptPath, shContent, { mode: 0o755 });
            return {
                scriptPath,
                command: '/bin/sh',
                args: [scriptPath]
            };
        }
    }
    // --- Private Helpers ---
    async fetchGitHubRelease() {
        // We query the recent releases list instead of just '/releases/latest'
        // because repository tags like 'Launcher' (for the installer) can mask newer version releases
        // like v1.0.37 on GitHub!
        try {
            const listUrl = `https://api.github.com/repos/${this.repoOwner}/${this.repoName}/releases?per_page=15`;
            const releases = await this.httpGetJson(listUrl, {
                'User-Agent': 'Aether-Launcher',
                Accept: 'application/vnd.github.v3+json'
            });
            if (Array.isArray(releases) && releases.length > 0) {
                // Filter releases that have a semantic version tag (e.g. v1.0.37) and an update asset
                const versionedReleases = releases.filter(r => {
                    if (!r.tag_name)
                        return false;
                    const isVersionTag = /^v?\d+(\.\d+)+/i.test(r.tag_name.trim());
                    if (!isVersionTag)
                        return false;
                    const hasUpdateAsset = Array.isArray(r.assets) && r.assets.some((a) => /aether.*\.zip$/i.test(a.name) || /\.zip$/i.test(a.name));
                    return hasUpdateAsset;
                });
                if (versionedReleases.length > 0) {
                    // Sort by version descending
                    versionedReleases.sort((a, b) => {
                        const verA = a.tag_name.replace(/^v/, '');
                        const verB = b.tag_name.replace(/^v/, '');
                        return AutoUpdater.compareVersions(verB, verA);
                    });
                    return versionedReleases[0];
                }
            }
        }
        catch {
            // Fallback to /releases/latest if list fails
        }
        const url = `https://api.github.com/repos/${this.repoOwner}/${this.repoName}/releases/latest`;
        const data = await this.httpGetJson(url, {
            'User-Agent': 'Aether-Launcher',
            Accept: 'application/vnd.github.v3+json'
        });
        return data;
    }
    async fetchLatestTagViaRedirect() {
        const url = `https://github.com/${this.repoOwner}/${this.repoName}/releases/latest`;
        const ca = getSystemCa();
        return new Promise(resolve => {
            https
                .get(url, { ca, headers: { 'User-Agent': 'Aether-Launcher' } }, res => {
                if (res.statusCode && res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
                    const loc = res.headers.location;
                    const match = loc.match(/\/tag\/([^/?#]+)/);
                    if (match && match[1]) {
                        return resolve(match[1]);
                    }
                }
                resolve(null);
            })
                .on('error', () => resolve(null));
        });
    }
    async fetchRawManifest() {
        const rawUrl = `https://raw.githubusercontent.com/${this.repoOwner}/${this.repoName}/main/update_manifest.json`;
        try {
            const data = await this.httpGetJson(rawUrl, { 'User-Agent': 'Aether-Launcher' });
            return data;
        }
        catch {
            // Fallback: GitHub Contents API
            const apiUrl = `https://api.github.com/repos/${this.repoOwner}/${this.repoName}/contents/update_manifest.json`;
            const apiData = await this.httpGetJson(apiUrl, {
                'User-Agent': 'Aether-Launcher',
                Accept: 'application/vnd.github.v3.raw'
            });
            return apiData;
        }
    }
    findBestAsset(assets) {
        if (!assets || assets.length === 0)
            return null;
        // Preference: 'Aether Update.zip', 'Aether.Update.zip', 'Aether.zip', any .zip, any .exe
        const patterns = [
            /^aether[ ._-]?update\.zip$/i,
            /^aether\.zip$/i,
            /\.zip$/i,
            /^aether[ ._-]?installer\.exe$/i,
            /\.exe$/i
        ];
        for (const pattern of patterns) {
            const found = assets.find(a => pattern.test(a.name));
            if (found)
                return found;
        }
        return assets[0];
    }
    buildManifestFromRelease(release, asset) {
        const version = release.tag_name ? release.tag_name.replace(/^v/, '').trim() : this.currentVersion;
        const notes = release.body || release.name || `Release ${version}`;
        const url = asset ? asset.browser_download_url : `https://github.com/${this.repoOwner}/${this.repoName}/releases/tag/${release.tag_name}`;
        const size = asset ? asset.size : undefined;
        return {
            version,
            title: release.name || `Aether ${version}`,
            title_en: release.name || `Aether ${version}`,
            title_ru: release.name || `Aether ${version}`,
            notes,
            notes_en: notes,
            notes_ru: notes,
            changes: notes.split('\n').map((l) => l.trim()).filter((l) => l.length > 0),
            changes_en: notes.split('\n').map((l) => l.trim()).filter((l) => l.length > 0),
            changes_ru: notes.split('\n').map((l) => l.trim()).filter((l) => l.length > 0),
            url,
            size
        };
    }
    httpGetJson(url, headers) {
        return new Promise((resolve, reject) => {
            const client = url.startsWith('https:') ? https : http;
            const ca = url.startsWith('https:') ? getSystemCa() : undefined;
            const req = client.get(url, { ca, headers }, res => {
                if (res.statusCode && res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
                    const redirectUrl = new URL(res.headers.location, url).toString();
                    return this.httpGetJson(redirectUrl, headers).then(resolve).catch(reject);
                }
                if (res.statusCode !== 200) {
                    return reject(new Error(`HTTP ${res.statusCode} ${res.statusMessage || ''}`));
                }
                let body = '';
                res.setEncoding('utf-8');
                res.on('data', chunk => (body += chunk));
                res.on('end', () => {
                    try {
                        const parsed = JSON.parse(body);
                        resolve(parsed);
                    }
                    catch (err) {
                        reject(new Error(`Invalid JSON received: ${err.message}`));
                    }
                });
            });
            req.on('error', reject);
            req.setTimeout(15000, () => req.destroy(new Error('Request timed out')));
        });
    }
    saveCachedManifest(manifest) {
        try {
            fs.writeFileSync(this.cachePath, JSON.stringify(manifest, null, 2), 'utf-8');
        }
        catch {
            // Ignore cache write error
        }
    }
    loadCachedManifest() {
        try {
            if (fs.existsSync(this.cachePath)) {
                const raw = fs.readFileSync(this.cachePath, 'utf-8');
                return JSON.parse(raw);
            }
        }
        catch {
            // Ignore cache read error
        }
        return null;
    }
}
