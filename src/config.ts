import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';

export type LauncherTheme = 'void-purple' | 'crimson' | 'cyan-neon' | 'emerald' | 'amber' | 'nordic-blue';
export type LauncherLanguage = 'ru' | 'en';
export type JvmPreset = 'aikar' | 'low-ram' | 'standard' | 'generative-zgc' | 'custom';

export interface LauncherConfig {
  version: string;
  storagePath: string;
  theme: LauncherTheme;
  language: LauncherLanguage;
  javaPath: string;
  allocatedMemory: number; // in MB
  jvmPreset: JvmPreset;
  customJvmArgs: string;
  lowCpuMode: boolean;
  autoMinimizeOnLaunch: boolean;
  closeOnLaunch: boolean;
  checkUpdatesOnStartup: boolean;
  autoDownloadUpdates: boolean;
  activeContainerId: string;
  offlineUsername: string;
  resolution: {
    width: number;
    height: number;
    fullscreen: boolean;
  };
}

export function getDefaultStoragePath(): string {
  if (process.env.AETHER_STORAGE) {
    return path.resolve(process.env.AETHER_STORAGE);
  }

  const isPortable = fs.existsSync(path.resolve('portable.txt')) || process.argv.includes('--portable');
  if (isPortable) {
    return path.resolve('data');
  }

  const home = os.homedir();
  if (process.platform === 'win32') {
    const appData = process.env.APPDATA || path.join(home, 'AppData', 'Roaming');
    return path.join(appData, 'AetherLauncher');
  } else if (process.platform === 'darwin') {
    return path.join(home, 'Library', 'Application Support', 'AetherLauncher');
  } else {
    const xdgData = process.env.XDG_DATA_HOME || path.join(home, '.local', 'share');
    return path.join(xdgData, 'aether-launcher');
  }
}

export function getDefaultConfig(): LauncherConfig {
  const totalMemMb = Math.round(os.totalmem() / (1024 * 1024));
  // Recommend 4GB if system has > 8GB, otherwise half of total RAM, min 2GB
  const recommendedMem = totalMemMb >= 8192 ? 4096 : Math.max(2048, Math.floor(totalMemMb / 2));

  return {
    version: '1.0.38',
    storagePath: getDefaultStoragePath(),
    theme: 'void-purple',
    language: 'ru',
    javaPath: '',
    allocatedMemory: recommendedMem,
    jvmPreset: 'aikar',
    customJvmArgs: '',
    lowCpuMode: true,
    autoMinimizeOnLaunch: true,
    closeOnLaunch: false,
    checkUpdatesOnStartup: true,
    autoDownloadUpdates: false,
    activeContainerId: '',
    offlineUsername: 'Player',
    resolution: {
      width: 925,
      height: 530,
      fullscreen: false
    }
  };
}

export class ConfigManager {
  private config: LauncherConfig;
  private configFilePath: string;

  constructor(customStoragePath?: string) {
    const storage = customStoragePath || getDefaultStoragePath();
    this.configFilePath = path.join(storage, 'config.json');
    this.config = this.load();
    if (customStoragePath) {
      this.config.storagePath = customStoragePath;
    }
  }

  public get(): LauncherConfig {
    return { ...this.config };
  }

  public update(partial: Partial<LauncherConfig>): LauncherConfig {
    this.config = {
      ...this.config,
      ...partial
    };
    this.save();
    return this.get();
  }

  public getStoragePath(): string {
    return this.config.storagePath;
  }

  public setStoragePath(newPath: string): void {
    this.config.storagePath = path.resolve(newPath);
    this.configFilePath = path.join(this.config.storagePath, 'config.json');
    this.save();
  }

  private load(): LauncherConfig {
    const defaults = getDefaultConfig();
    try {
      if (fs.existsSync(this.configFilePath)) {
        const raw = fs.readFileSync(this.configFilePath, 'utf-8');
        const parsed = JSON.parse(raw);
        return {
          ...defaults,
          ...parsed,
          resolution: {
            ...defaults.resolution,
            ...(parsed.resolution || {})
          }
        };
      }
    } catch (err) {
      console.warn(`[Config] Failed to read ${this.configFilePath}, falling back to defaults:`, err);
    }
    return defaults;
  }

  private save(): void {
    try {
      const dir = path.dirname(this.configFilePath);
      if (!fs.existsSync(dir)) {
        fs.mkdirSync(dir, { recursive: true });
      }
      const tmpPath = `${this.configFilePath}.tmp.${Date.now()}`;
      fs.writeFileSync(tmpPath, JSON.stringify(this.config, null, 2), 'utf-8');
      fs.renameSync(tmpPath, this.configFilePath);
    } catch (err) {
      console.error(`[Config] Failed to save config to ${this.configFilePath}:`, err);
    }
  }
}
