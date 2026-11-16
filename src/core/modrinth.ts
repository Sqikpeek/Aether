import fs from 'node:fs';
import path from 'node:path';
import https from 'node:https';
import crypto from 'node:crypto';
import { getSystemCa } from './http_utils.js';

export type ModrinthProjectType = 'mod' | 'modpack' | 'datapack' | 'resourcepack' | 'plugin';

export interface ModrinthSearchResult {
  project_id: string;
  project_type: ModrinthProjectType;
  slug: string;
  author: string;
  title: string;
  description: string;
  categories: string[];
  display_categories: string[];
  versions: string[];
  downloads: number;
  follows: number;
  icon_url: string;
  date_created: string;
  date_modified: string;
  latest_version: string;
  license: string;
}

export interface ModrinthVersionFile {
  hashes: {
    sha1?: string;
    sha512?: string;
  };
  url: string;
  filename: string;
  primary: boolean;
  size: number;
}

export interface ModrinthVersionDependency {
  version_id?: string;
  project_id?: string;
  file_name?: string;
  dependency_type: 'required' | 'optional' | 'incompatible' | 'embedded';
}

export interface ModrinthProjectVersion {
  id: string;
  project_id: string;
  author_id: string;
  name: string;
  version_number: string;
  game_versions: string[];
  loaders: string[];
  featured: boolean;
  files: ModrinthVersionFile[];
  dependencies: ModrinthVersionDependency[];
  date_published: string;
  downloads: number;
}

export interface SearchOptions {
  query?: string;
  facets?: string[][]; // e.g. [["categories:fabric"], ["versions:1.21.1"], ["project_type:mod"]]
  index?: 'relevance' | 'downloads' | 'follows' | 'newest' | 'updated';
  offset?: number;
  limit?: number;
}

export class ModrinthClient {
  private baseUrl = 'https://api.modrinth.com/v2';
  private userAgent = 'AetherLauncher/1.0.38 (github.com/Sqikpeek/Aether)';
  private searchCache = new Map<string, { data: any; timestamp: number }>();
  private cacheTtlMs = 60000; // 1 minute cache

  public async search(options: SearchOptions = {}): Promise<{
    hits: ModrinthSearchResult[];
    offset: number;
    limit: number;
    total_hits: number;
  }> {
    const params = new URLSearchParams();
    if (options.query) params.set('query', options.query);
    if (options.index) params.set('index', options.index);
    if (options.offset !== undefined) params.set('offset', options.offset.toString());
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

    const data = await this.getJson<any>(url);
    this.searchCache.set(cacheKey, { data, timestamp: Date.now() });
    return data;
  }

  public async getProjectVersions(
    projectIdOrSlug: string,
    loaders?: string[],
    gameVersions?: string[]
  ): Promise<ModrinthProjectVersion[]> {
    const params = new URLSearchParams();
    if (loaders && loaders.length > 0) {
      params.set('loaders', JSON.stringify(loaders));
    }
    if (gameVersions && gameVersions.length > 0) {
      params.set('game_versions', JSON.stringify(gameVersions));
    }

    const qs = params.toString();
    const url = `${this.baseUrl}/project/${encodeURIComponent(projectIdOrSlug)}/version${qs ? `?${qs}` : ''}`;
    return this.getJson<ModrinthProjectVersion[]>(url);
  }

  public async getProject(projectIdOrSlug: string): Promise<any> {
    const url = `${this.baseUrl}/project/${encodeURIComponent(projectIdOrSlug)}`;
    return this.getJson<any>(url);
  }

  public async getVersion(versionId: string): Promise<ModrinthProjectVersion> {
    const url = `${this.baseUrl}/version/${encodeURIComponent(versionId)}`;
    return this.getJson<ModrinthProjectVersion>(url);
  }

  /**
   * Installs a selected version and its complete required-dependency tree.
   *
   * Every dependency, including a dependency referenced by `version_id`, is
   * checked against the active container's exact Minecraft version and loader.
   * Project-only dependencies are resolved with those same filters. Optional,
   * incompatible and embedded entries are not downloaded as separate jars.
   */
  public async installVersionWithDependencies(
    version: ModrinthProjectVersion,
    targetDir: string,
    compatibility: { loaders?: string[]; gameVersions?: string[] } = {},
    onProgress?: (bytes: number, total: number) => void
  ): Promise<{ installed: Array<{ fileName: string; filePath: string }>; dependencyCount: number }> {
    const installed: Array<{ fileName: string; filePath: string }> = [];
    const visited = new Set<string>();
    const compatibleWithContainer = (candidate: ModrinthProjectVersion): boolean => {
      const versionOk = !compatibility.gameVersions?.length ||
        compatibility.gameVersions.some(version => candidate.game_versions.includes(version));
      const candidateLoaders = candidate.loaders.map(loader => loader.toLowerCase());
      const loaderOk = !compatibility.loaders?.length ||
        compatibility.loaders.some(loader => candidateLoaders.includes(loader.toLowerCase()));
      return versionOk && loaderOk;
    };

    if (!compatibleWithContainer(version)) {
      throw new Error(`Selected mod version ${version.version_number} does not match the active container`);
    }

    const installTree = async (current: ModrinthProjectVersion): Promise<void> => {
      if (visited.has(current.id)) return;
      // Mark before walking children to handle circular/duplicate dependency references.
      visited.add(current.id);

      for (const dependency of current.dependencies || []) {
        if (dependency.dependency_type !== 'required') continue;

        let dependencyVersion: ModrinthProjectVersion | undefined;
        if (dependency.version_id) {
          dependencyVersion = await this.getVersion(dependency.version_id);
        } else if (dependency.project_id) {
          const candidates = await this.getProjectVersions(
            dependency.project_id,
            compatibility.loaders,
            compatibility.gameVersions
          );
          dependencyVersion = candidates[0];
        }

        if (!dependencyVersion || !compatibleWithContainer(dependencyVersion)) {
          throw new Error(`No compatible required dependency could be resolved for ${current.name}`);
        }
        await installTree(dependencyVersion);
      }

      installed.push(await this.installVersion(current, targetDir, onProgress));
    };

    await installTree(version);
    return {
      installed,
      dependencyCount: Math.max(0, installed.length - 1)
    };
  }

  /**
   * Installs a mod or mod version into the target directory.
   */
  public async installVersion(
    version: ModrinthProjectVersion,
    targetDir: string,
    onProgress?: (bytes: number, total: number) => void
  ): Promise<{ fileName: string; filePath: string }> {
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

  private downloadFile(
    fileUrl: string,
    destPath: string,
    expectedSha512?: string,
    onProgress?: (bytes: number, total: number) => void
  ): Promise<void> {
    const tempPath = `${destPath}.tmp.${Date.now()}`;
    const fileStream = fs.createWriteStream(tempPath);
    const hash = expectedSha512 ? crypto.createHash('sha512') : null;

    return new Promise((resolve, reject) => {
      const handleReq = (urlStr: string, redirects = 0) => {
        if (redirects > 5) {
          fileStream.close();
          fs.unlink(tempPath, () => {});
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
              fs.unlink(tempPath, () => {});
              return reject(new Error(`Failed to download mod: HTTP ${res.statusCode}`));
            }

            const total = parseInt(res.headers['content-length'] || '0', 10);
            let downloaded = 0;

            res.on('data', chunk => {
              downloaded += chunk.length;
              if (hash) hash.update(chunk);
              fileStream.write(chunk);
              if (onProgress) onProgress(downloaded, total);
            });

            res.on('end', () => {
              fileStream.end(() => {
                if (hash && expectedSha512) {
                  const calculated = hash.digest('hex').toLowerCase();
                  if (calculated !== expectedSha512.toLowerCase()) {
                    fs.unlink(tempPath, () => {});
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
              fs.unlink(tempPath, () => {});
              reject(err);
            });
          })
          .on('error', err => {
            fileStream.close();
            fs.unlink(tempPath, () => {});
            reject(err);
          });
      };

      handleReq(fileUrl);
    });
  }

  private getJson<T>(url: string): Promise<T> {
    return new Promise((resolve, reject) => {
      const ca = getSystemCa();
      https
        .get(url, { ca, headers: { 'User-Agent': this.userAgent, Accept: 'application/json' } }, res => {
          if (res.statusCode && res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
            const redirect = new URL(res.headers.location, url).toString();
            return this.getJson<T>(redirect).then(resolve).catch(reject);
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
            } catch (err: any) {
              reject(new Error(`Failed to parse Modrinth JSON: ${err.message}`));
            }
          });
        })
        .on('error', reject);
    });
  }
}
