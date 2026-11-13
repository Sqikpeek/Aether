export interface BackupItem {
    id: string;
    containerId: string;
    name: string;
    createdAt: number;
    sizeBytes: number;
    filePath: string;
}
export declare class BackupManager {
    private backupsDir;
    constructor(storagePath: string);
    listBackups(containerId?: string): BackupItem[];
    /**
     * Creates a backup of container saves and config folders.
     */
    createBackup(containerId: string, containerDir: string, backupName?: string): Promise<BackupItem>;
    deleteBackup(id: string): boolean;
    private collectFilesRecursive;
}
