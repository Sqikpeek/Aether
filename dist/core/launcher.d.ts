import { EventEmitter } from 'node:events';
import { ContainerConfig } from './containers.js';
export type LauncherState = 'idle' | 'preparing' | 'downloading' | 'running' | 'stopping' | 'crashed';
export interface GameLogEntry {
    timestamp: string;
    level: 'INFO' | 'WARN' | 'ERROR' | 'FATAL' | 'DEBUG';
    message: string;
}
export interface LaunchOptions {
    container: ContainerConfig;
    containerDir: string;
    storageDir: string;
    javaPath?: string;
    allocatedMemoryMb?: number;
    jvmPreset?: any;
    customJvmArgs?: string;
    username?: string;
    resolution?: {
        width: number;
        height: number;
        fullscreen: boolean;
    };
}
export declare class GameLauncher extends EventEmitter {
    private currentState;
    private activeProcess;
    private activeContainerId;
    private launchStartTime;
    private logBuffer;
    private maxLogLines;
    private logStream;
    private mojangVersionsCache;
    constructor();
    getState(): LauncherState;
    getActiveContainerId(): string;
    getRecentLogs(): GameLogEntry[];
    clearLogs(): void;
    /**
     * Fetches official list of verified Minecraft release versions from Mojang.
     */
    getAvailableMinecraftVersions(): Promise<string[]>;
    /**
     * Launches Minecraft for the specified container.
     * Event-driven with 0.00% background CPU usage while running.
     */
    launch(options: LaunchOptions): Promise<void>;
    /**
     * Gracefully or forcefully stops running game process.
     */
    stopGame(): void;
    private handleProcessOutput;
    private handleProcessExit;
    private buildGameArguments;
    private addLog;
    private setState;
    private fetchJson;
}
