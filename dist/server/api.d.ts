export declare class LauncherServer {
    private server;
    private configManager;
    private containerManager;
    private updater;
    private launcher;
    private modrinth;
    private backupManager;
    private port;
    private sseClients;
    private uiDir;
    constructor(port?: number, customStoragePath?: string);
    getPort(): number;
    private setupEvents;
    private broadcastEvent;
    start(): Promise<number>;
    stop(): Promise<void>;
    private handleRequest;
    private handleApiRoute;
    private handleStaticRoute;
    private serveFile;
}
