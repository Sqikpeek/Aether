import fs from 'node:fs';
import path from 'node:path';
import https from 'node:https';
import crypto from 'node:crypto';
import { getSystemCa } from './http_utils.js';
export class ModrinthClient {
    baseUrl = 'https://api.modrinth.com/v2';
    userAgent = 'AetherLauncher/1.0.38 (github.com/Sqikpeek/Aether)';
    searchCache = new Map();
    cacheTtlMs = 60000; // 1 minute cache
    async search(options = {}) {
        const params = new URLSearchParams();
        if (options.query)
            params.set('query', options.query);
        if (options.index)
            params.set('index', options.index);
        if (options.offset !== undefined)
            params.set('offset', options.offset.toString());
        params.set('limit', (options.limit || 20).toString());
        if (options.facets && options.facets.length > 0) {
            params.set('facets', JSON.stringify(options.facets));
        }
        const url = `${this.baseUrl}/search?${params.toString()}`;
        const cacheKey = url;
        const cached = this.searchCache.get(cacheKey);
        if (cached && Date.now() - cached.timestamp < this.cacheTtlMs) {
            return cached.data;
        }
        const data = await this.getJson(url);
        this.searchCache.set(cacheKey, { data, timestamp: Date.now() });
        return data;
    }
    async getProjectVersions(projectIdOrSlug, loaders, gameVersions) {
        const params = new URLSearchParams();
        if (loaders && loaders.length > 0) {
            params.set('loaders', JSON.stringify(loaders));
        }
        if (gameVersions && gameVersions.length > 0) {
            params.set('game_versions', JSON.stringify(gameVersions));
        }
        const qs = params.toString();
        const url = `${this.baseUrl}/project/${encodeURIComponent(projectIdOrSlug)}/version${qs ? `?${qs}` : ''}`;
        return this.getJson(url);
    }
    async getProject(projectIdOrSlug) {
        const url = `${this.baseUrl}/project/${encodeURIComponent(projectIdOrSlug)}`;
        return this.getJson(url);
    }
    /**
     * Installs a mod or mod version into the target directory.
     */
    async installVersion(version, targetDir, onProgress) {
        if (!fs.existsSync(targetDir)) {
            fs.mkdirSync(targetDir, { recursive: true });
        }
        const primaryFile = version.files.find(f => f.primary) || version.files[0];
        if (!primaryFile) {
            throw new Error(`Version ${version.version_number} has no downloadable files`);
        }
        const destPath = path.join(targetDir, primaryFile.filename);
        await this.downloadFile(primaryFile.url, destPath, primaryFile.hashes?.sha512, onProgress);
        return {
            fileName: primaryFile.filename,
            filePath: destPath
        };
    }
    downloadFile(fileUrl, destPath, expectedSha512, onProgress) {
        const tempPath = `${destPath}.tmp.${Date.now()}`;
        const fileStream = fs.createWriteStream(tempPath);
        const hash = expectedSha512 ? crypto.createHash('sha512') : null;
        return new Promise((resolve, reject) => {
            const handleReq = (urlStr, redirects = 0) => {
                if (redirects > 5) {
                    fileStream.close();
                    fs.unlink(tempPath, () => { });
                    return reject(new Error('Too many redirects downloading mod'));
                }
                const ca = getSystemCa();
                https
                    .get(urlStr, { ca, headers: { 'User-Agent': this.userAgent } }, res => {
                    if (res.statusCode && res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
                        const redirect = new URL(res.headers.location, urlStr).toString();
                        return handleReq(redirect, redirects + 1);
                    }
                    if (res.statusCode !== 200) {
                        fileStream.close();
                        fs.unlink(tempPath, () => { });
                        return reject(new Error(`Failed to download mod: HTTP ${res.statusCode}`));
                    }
                    const total = parseInt(res.headers['content-length'] || '0', 10);
                    let downloaded = 0;
                    res.on('data', chunk => {
                        downloaded += chunk.length;
                        if (hash)
                            hash.update(chunk);
                        fileStream.write(chunk);
                        if (onProgress)
                            onProgress(downloaded, total);
                    });
                    res.on('end', () => {
                        fileStream.end(() => {
                            if (hash && expectedSha512) {
                                const calculated = hash.digest('hex').toLowerCase();
                                if (calculated !== expectedSha512.toLowerCase()) {
                                    fs.unlink(tempPath, () => { });
                                    return reject(new Error('SHA-512 verification failed for downloaded mod'));
                                }
                            }
                            if (fs.existsSync(destPath)) {
                                fs.unlinkSync(destPath);
                            }
                            fs.renameSync(tempPath, destPath);
                            resolve();
                        });
                    });
                    res.on('error', err => {
                        fileStream.close();
                        fs.unlink(tempPath, () => { });
                        reject(err);
                    });
                })
                    .on('error', err => {
                    fileStream.close();
                    fs.unlink(tempPath, () => { });
                    reject(err);
                });
            };
            handleReq(fileUrl);
        });
    }
    getJson(url) {
        return new Promise((resolve, reject) => {
            const ca = getSystemCa();
            https
                .get(url, { ca, headers: { 'User-Agent': this.userAgent, Accept: 'application/json' } }, res => {
                if (res.statusCode && res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
                    const redirect = new URL(res.headers.location, url).toString();
                    return this.getJson(redirect).then(resolve).catch(reject);
                }
                if (res.statusCode !== 200) {
                    return reject(new Error(`Modrinth API returned HTTP ${res.statusCode}`));
                }
                let data = '';
                res.setEncoding('utf-8');
                res.on('data', chunk => (data += chunk));
                res.on('end', () => {
                    try {
                        resolve(JSON.parse(data));
                    }
                    catch (err) {
                        reject(new Error(`Failed to parse Modrinth JSON: ${err.message}`));
                    }
                });
            })
                .on('error', reject);
        });
    }
}
