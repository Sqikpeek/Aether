export type LoaderType = 'vanilla' | 'fabric' | 'quilt' | 'forge' | 'neoforge';
export interface ContainerConfig {
    id: string;
    name: string;
    minecraftVersion: string;
    loaderType: LoaderType;
    loaderVersion?: string;
    icon?: string;
    memoryMin?: number;
    memoryMax?: number;
    jvmArgsOverride?: string;
    shareSaves: boolean;
    created: number;
    lastPlayed?: number;
    playTimeMinutes?: number;
}
export interface InstalledMod {
    id: string;
    fileName: string;
    name: string;
    version: string;
    enabled: boolean;
    fileSizeBytes: number;
    modifiedTime: number;
}
export declare class ContainerManager {
    private storagePath;
    private containersDir;
    private sharedSavesDir;
    private metadataPath;
    private containers;
    constructor(storagePath: string);
    private initDirectories;
    listContainers(): ContainerConfig[];
    getContainer(id: string): ContainerConfig | null;
    getContainerDir(id: string): string;
    getContainerModsDir(id: string): string;
    getContainerSavesDir(id: string): string;
    createContainer(params: {
        name: string;
        minecraftVersion: string;
        loaderType?: LoaderType;
        loaderVersion?: string;
        icon?: string;
        memoryMin?: number;
        memoryMax?: number;
        jvmArgsOverride?: string;
        shareSaves?: boolean;
    }): ContainerConfig;
    updateContainer(id: string, partial: Partial<ContainerConfig>): ContainerConfig | null;
    cloneContainer(id: string, newName?: string): ContainerConfig | null;
    deleteContainer(id: string): boolean;
    recordPlaySession(id: string, durationMinutes: number): void;
    /**
     * Sets up or removes directory link for shared saves.
     * On Windows, uses Directory Junction (mklink /J) which does NOT need admin rights.
     * On POSIX, uses standard directory symlink.
     */
    setupSavesLink(id: string, shareSaves: boolean): void;
    /**
     * Scans container's mods folder and returns list of installed mods (both .jar and .jar.disabled).
     */
    listInstalledMods(id: string): InstalledMod[];
    /**
     * Toggles a mod between enabled (.jar) and disabled (.jar.disabled) instantly without re-downloading.
     */
    toggleMod(id: string, fileName: string): boolean;
    deleteMod(id: string, fileName: string): boolean;
    private formatModName;
    private extractModVersion;
    private copyDirRecursive;
    private loadContainers;
    private saveContainers;
}
