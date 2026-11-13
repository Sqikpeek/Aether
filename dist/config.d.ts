export type LauncherTheme = 'void-purple' | 'crimson' | 'cyan-neon' | 'emerald' | 'amber' | 'nordic-blue';
export type LauncherLanguage = 'ru' | 'en';
export type JvmPreset = 'aikar' | 'low-ram' | 'standard' | 'generative-zgc' | 'custom';
export interface LauncherConfig {
    version: string;
    storagePath: string;
    theme: LauncherTheme;
    language: LauncherLanguage;
    javaPath: string;
    allocatedMemory: number;
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
export declare function getDefaultStoragePath(): string;
export declare function getDefaultConfig(): LauncherConfig;
export declare class ConfigManager {
    private config;
    private configFilePath;
    constructor(customStoragePath?: string);
    get(): LauncherConfig;
    update(partial: Partial<LauncherConfig>): LauncherConfig;
    getStoragePath(): string;
    setStoragePath(newPath: string): void;
    private load;
    private save;
}
