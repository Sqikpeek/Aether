import { EventEmitter } from 'node:events';
export interface UpdateManifest {
    version: string;
    title?: string;
    title_en?: string;
    title_ru?: string;
    notes?: string;
    notes_en?: string;
    notes_ru?: string;
    changes?: string[];
    changes_en?: string[];
    changes_ru?: string[];
    url: string;
    size?: number;
    sha256?: string;
}
export interface UpdateCheckResult {
    hasUpdate: boolean;
    currentVersion: string;
    latestVersion: string;
    manifest: UpdateManifest | null;
    error?: string;
    source?: 'github-api' | 'github-redirect' | 'raw-manifest' | 'cached';
}
export interface DownloadProgress {
    bytesDownloaded: number;
    totalBytes: number;
    percent: number;
    speedBytesPerSec: number;
    etaSeconds: number;
}
export declare class AutoUpdater extends EventEmitter {
    private currentVersion;
    private repoOwner;
    private repoName;
    private cachePath;
    constructor(currentVersion?: string, repoOwner?: string, repoName?: string, cacheDir?: string);
    getCurrentVersion(): string;
    /**
     * Compares two semantic version strings (e.g. "1.0.38" vs "1.0.37").
     * Returns:
     *   1 if v1 > v2
     *  -1 if v1 < v2
     *   0 if v1 === v2
     */
    static compareVersions(v1: string, v2: string): number;
    /**
     * Checks for updates using a multi-tier fallback mechanism:
     * 1. GitHub Releases API (/repos/{owner}/{repo}/releases/latest)
     * 2. GitHub Releases Redirect (/releases/latest HTTP 302 location)
     * 3. Repository Raw Manifest (update_manifest.json)
     * 4. Cached manifest if offline
     */
    checkForUpdates(): Promise<UpdateCheckResult>;
    /**
     * Downloads update archive with streaming SHA-256 calculation and progress reporting.
     */
    downloadUpdate(downloadUrl: string, destinationPath: string, expectedSha256?: string, expectedSize?: number, onProgress?: (progress: DownloadProgress) => void): Promise<{
        path: string;
        verifiedSha256: boolean;
        calculatedSha256: string;
    }>;
    /**
     * Generates a platform-specific updater script that safely waits for the launcher
     * process to exit, backs up old files, unpacks new files, cleans environment variables,
     * and restarts the launcher hidden without console flash.
     */
    generateUpdateScript(archivePath: string, installDir: string, targetExeName?: string, parentPid?: number): {
        scriptPath: string;
        command: string;
        args: string[];
    };
    private fetchGitHubRelease;
    private fetchLatestTagViaRedirect;
    private fetchRawManifest;
    private findBestAsset;
    private buildManifestFromRelease;
    private httpGetJson;
    private saveCachedManifest;
    private loadCachedManifest;
}
