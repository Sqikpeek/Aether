import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { ConfigManager } from '../config.js';
import { ContainerManager } from '../core/containers.js';
import { AutoUpdater } from '../core/updater.js';
import { GameLauncher } from '../core/launcher.js';
import { ModrinthClient } from '../core/modrinth.js';
import { JavaDetector } from '../core/java.js';
import { ServerPinger } from '../core/server_pinger.js';
import { BackupManager } from '../core/backup.js';
import { CrashAnalyzer } from '../core/crash_analyzer.js';
export class LauncherServer {
    server = null;
    configManager;
    containerManager;
    updater;
    launcher;
    modrinth;
    backupManager;
    port;
    sseClients = new Set();
    uiDir;
    constructor(port = 3000, customStoragePath) {
        this.port = port;
        this.configManager = new ConfigManager(customStoragePath);
        const storagePath = this.configManager.getStoragePath();
        this.containerManager = new ContainerManager(storagePath);
        this.updater = new AutoUpdater('1.0.38', 'Sqikpeek', 'Aether', storagePath);
        this.launcher = new GameLauncher();
        this.modrinth = new ModrinthClient();
        this.backupManager = new BackupManager(storagePath);
        // Locate UI directory
        const __filename = fileURLToPath(import.meta.url);
        const __dirname = path.dirname(__filename);
        const possibleUiDirs = [
            path.join(__dirname, '../ui'),
            path.join(__dirname, '../../src/ui'),
            path.join(process.cwd(), 'src/ui'),
            path.join(process.cwd(), 'ui')
        ];
        this.uiDir = possibleUiDirs.find(d => fs.existsSync(path.join(d, 'index.html'))) || possibleUiDirs[0];
        this.setupEvents();
    }
    getPort() {
        return this.port;
    }
    setupEvents() {
        this.launcher.on('state_changed', state => {
            this.broadcastEvent('state', { state, containerId: this.launcher.getActiveContainerId() });
        });
        this.launcher.on('log_entry', entry => {
            this.broadcastEvent('log', entry);
        });
        this.launcher.on('game_crashed', data => {
            this.broadcastEvent('crashed', data);
        });
        this.launcher.on('game_stopped', data => {
            this.broadcastEvent('stopped', data);
        });
        this.updater.on('progress', progress => {
            this.broadcastEvent('update_progress', progress);
        });
    }
    broadcastEvent(type, data) {
        const payload = `event: ${type}\ndata: ${JSON.stringify(data)}\n\n`;
        for (const client of this.sseClients) {
            try {
                client.write(payload);
            }
            catch {
                this.sseClients.delete(client);
            }
        }
    }
    start() {
        return new Promise((resolve, reject) => {
            this.server = http.createServer((req, res) => {
                this.handleRequest(req, res).catch(err => {
                    console.error('[Server] Unhandled request error:', err);
                    if (!res.headersSent) {
                        res.writeHead(500, { 'Content-Type': 'application/json' });
                        res.end(JSON.stringify({ error: err.message }));
                    }
                });
            });
            this.server.listen(this.port, '0.0.0.0', () => {
                const addr = this.server?.address();
                const actualPort = typeof addr === 'object' && addr ? addr.port : this.port;
                this.port = actualPort;
                console.log(`[Aether Launcher] Server listening at http://0.0.0.0:${this.port}`);
                resolve(this.port);
            });
            this.server.on('error', reject);
        });
    }
    stop() {
        return new Promise(resolve => {
            for (const client of this.sseClients) {
                try {
                    client.end();
                }
                catch { }
            }
            this.sseClients.clear();
            if (this.server) {
                this.server.close(() => resolve());
            }
            else {
                resolve();
            }
        });
    }
    async handleRequest(req, res) {
        // CORS headers for local/preview access
        res.setHeader('Access-Control-Allow-Origin', '*');
        res.setHeader('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS');
        res.setHeader('Access-Control-Allow-Headers', 'Content-Type, Authorization');
        if (req.method === 'OPTIONS') {
            res.writeHead(204);
            res.end();
            return;
        }
        const url = new URL(req.url || '/', `http://${req.headers.host || 'localhost'}`);
        const pathname = url.pathname;
        // Server-Sent Events endpoint
        if (pathname === '/api/events') {
            res.writeHead(200, {
                'Content-Type': 'text/event-stream',
                'Cache-Control': 'no-cache, no-transform',
                Connection: 'keep-alive'
            });
            res.write(': keepalive\n\n');
            this.sseClients.add(res);
            req.on('close', () => {
                this.sseClients.delete(res);
            });
            return;
        }
        // API Routes
        if (pathname.startsWith('/api/')) {
            return this.handleApiRoute(req, res, pathname, url);
        }
        // Static UI Files
        return this.handleStaticRoute(req, res, pathname);
    }
    async handleApiRoute(req, res, pathname, url) {
        const method = req.method;
        const jsonResponse = (data, statusCode = 200) => {
            res.writeHead(statusCode, { 'Content-Type': 'application/json; charset=utf-8' });
            res.end(JSON.stringify(data));
        };
        const parseBody = async () => {
            return new Promise((resolve, reject) => {
                let body = '';
                req.on('data', chunk => (body += chunk));
                req.on('end', () => {
                    try {
                        resolve(body ? JSON.parse(body) : {});
                    }
                    catch (err) {
                        reject(new Error(`Invalid JSON body: ${err.message}`));
                    }
                });
                req.on('error', reject);
            });
        };
        // --- App Lifecycle ---
        if (pathname === '/api/heartbeat') {
            return jsonResponse({ status: 'alive', time: Date.now() });
        }
        if (pathname === '/api/app/quit' && method === 'POST') {
            jsonResponse({ success: true, message: 'Shutting down Aether Launcher...' });
            setTimeout(() => {
                this.stop().then(() => process.exit(0));
            }, 200);
            return;
        }
        // --- Config ---
        if (pathname === '/api/config' && method === 'GET') {
            return jsonResponse(this.configManager.get());
        }
        if (pathname === '/api/config' && method === 'POST') {
            const body = await parseBody();
            const updated = this.configManager.update(body);
            return jsonResponse(updated);
        }
        // --- Containers ---
        if (pathname === '/api/containers' && method === 'GET') {
            return jsonResponse(this.containerManager.listContainers());
        }
        if (pathname === '/api/containers' && method === 'POST') {
            const body = await parseBody();
            const created = this.containerManager.createContainer(body);
            return jsonResponse(created, 201);
        }
        const containerIdMatch = pathname.match(/^\/api\/containers\/([a-zA-Z0-9_-]+)$/);
        if (containerIdMatch) {
            const id = containerIdMatch[1];
            if (method === 'GET') {
                const c = this.containerManager.getContainer(id);
                if (!c)
                    return jsonResponse({ error: 'Container not found' }, 404);
                return jsonResponse(c);
            }
            if (method === 'PUT') {
                const body = await parseBody();
                const updated = this.containerManager.updateContainer(id, body);
                if (!updated)
                    return jsonResponse({ error: 'Container not found' }, 404);
                return jsonResponse(updated);
            }
            if (method === 'DELETE') {
                const ok = this.containerManager.deleteContainer(id);
                return jsonResponse({ success: ok });
            }
        }
        const cloneMatch = pathname.match(/^\/api\/containers\/([a-zA-Z0-9_-]+)\/clone$/);
        if (cloneMatch && method === 'POST') {
            const id = cloneMatch[1];
            const body = await parseBody();
            const cloned = this.containerManager.cloneContainer(id, body.name);
            if (!cloned)
                return jsonResponse({ error: 'Failed to clone container' }, 400);
            return jsonResponse(cloned, 201);
        }
        // --- Mods in Container ---
        const modsMatch = pathname.match(/^\/api\/containers\/([a-zA-Z0-9_-]+)\/mods$/);
        if (modsMatch && method === 'GET') {
            const id = modsMatch[1];
            const mods = this.containerManager.listInstalledMods(id);
            return jsonResponse(mods);
        }
        const toggleModMatch = pathname.match(/^\/api\/containers\/([a-zA-Z0-9_-]+)\/mods\/toggle$/);
        if (toggleModMatch && method === 'POST') {
            const id = toggleModMatch[1];
            const body = await parseBody();
            const ok = this.containerManager.toggleMod(id, body.fileName);
            return jsonResponse({ success: ok, mods: this.containerManager.listInstalledMods(id) });
        }
        const deleteModMatch = pathname.match(/^\/api\/containers\/([a-zA-Z0-9_-]+)\/mods\/delete$/);
        if (deleteModMatch && method === 'POST') {
            const id = deleteModMatch[1];
            const body = await parseBody();
            const ok = this.containerManager.deleteMod(id, body.fileName);
            return jsonResponse({ success: ok, mods: this.containerManager.listInstalledMods(id) });
        }
        // --- Modrinth ---
        if (pathname === '/api/modrinth/search' && method === 'GET') {
            const query = url.searchParams.get('query') || '';
            const loader = url.searchParams.get('loader');
            const gameVersion = url.searchParams.get('version');
            const projectType = url.searchParams.get('type') || 'mod';
            const facets = [
                [`project_type:${projectType}`]
            ];
            if (loader && loader !== 'vanilla')
                facets.push([`categories:${loader}`]);
            if (gameVersion)
                facets.push([`versions:${gameVersion}`]);
            const results = await this.modrinth.search({
                query,
                facets,
                limit: parseInt(url.searchParams.get('limit') || '20', 10),
                offset: parseInt(url.searchParams.get('offset') || '0', 10)
            });
            return jsonResponse(results);
        }
        if (pathname === '/api/modrinth/versions' && method === 'GET') {
            const projectId = url.searchParams.get('projectId') || '';
            const loader = url.searchParams.get('loader');
            const gameVersion = url.searchParams.get('version');
            const loaders = loader && loader !== 'vanilla' ? [loader] : undefined;
            const gameVersions = gameVersion ? [gameVersion] : undefined;
            const versions = await this.modrinth.getProjectVersions(projectId, loaders, gameVersions);
            return jsonResponse(versions);
        }
        if (pathname === '/api/modrinth/install' && method === 'POST') {
            const body = await parseBody();
            const container = this.containerManager.getContainer(body.containerId);
            if (!container)
                return jsonResponse({ error: 'Container not found' }, 404);
            const targetDir = this.containerManager.getContainerModsDir(body.containerId);
            const installed = await this.modrinth.installVersion(body.version, targetDir);
            return jsonResponse(installed);
        }
        // --- Java Detection ---
        if (pathname === '/api/java' && method === 'GET') {
            const runtimes = JavaDetector.detectJavaInstallations();
            return jsonResponse(runtimes);
        }
        // --- Minecraft Versions ---
        if (pathname === '/api/minecraft/versions' && method === 'GET') {
            const versions = await this.launcher.getAvailableMinecraftVersions();
            return jsonResponse(versions);
        }
        // --- Updater ---
        if (pathname === '/api/updater/check' && method === 'GET') {
            const result = await this.updater.checkForUpdates();
            return jsonResponse(result);
        }
        if (pathname === '/api/updater/download' && method === 'POST') {
            const body = await parseBody();
            const storage = this.configManager.getStoragePath();
            const dest = path.join(storage, 'updates', 'Aether_update.zip');
            const downloadResult = await this.updater.downloadUpdate(body.url, dest, body.sha256, body.size, p => this.broadcastEvent('update_progress', p));
            return jsonResponse(downloadResult);
        }
        // --- Game Launcher Controls ---
        if (pathname === '/api/launcher/launch' && method === 'POST') {
            const body = await parseBody();
            const container = this.containerManager.getContainer(body.containerId);
            if (!container)
                return jsonResponse({ error: 'Container not found' }, 404);
            const cfg = this.configManager.get();
            const containerDir = this.containerManager.getContainerDir(container.id);
            // Trigger launch asynchronously so HTTP response is returned immediately
            this.launcher
                .launch({
                container,
                containerDir,
                storageDir: cfg.storagePath,
                javaPath: cfg.javaPath,
                allocatedMemoryMb: cfg.allocatedMemory,
                jvmPreset: cfg.jvmPreset,
                customJvmArgs: cfg.customJvmArgs,
                username: cfg.offlineUsername,
                resolution: cfg.resolution
            })
                .catch(err => {
                console.error('[Launcher] Launch error:', err);
            });
            return jsonResponse({
                status: 'launching',
                containerId: container.id
            });
        }
        if (pathname === '/api/launcher/stop' && method === 'POST') {
            this.launcher.stopGame();
            return jsonResponse({ status: 'stopping' });
        }
        if (pathname === '/api/launcher/status' && method === 'GET') {
            return jsonResponse({
                state: this.launcher.getState(),
                activeContainerId: this.launcher.getActiveContainerId(),
                logs: this.launcher.getRecentLogs()
            });
        }
        if (pathname === '/api/launcher/diagnostic' && method === 'GET') {
            const cid = url.searchParams.get('containerId') || this.launcher.getActiveContainerId();
            if (cid) {
                const cdir = this.containerManager.getContainerDir(cid);
                const diag = CrashAnalyzer.analyzeContainerLogs(cdir);
                return jsonResponse(diag);
            }
            return jsonResponse({ hasIssue: false, type: 'none' });
        }
        // --- Server Ping ---
        if (pathname === '/api/servers/ping' && method === 'POST') {
            const body = await parseBody();
            const pingRes = await ServerPinger.ping(body.host, body.port || 25565);
            return jsonResponse(pingRes);
        }
        // --- Backups ---
        if (pathname === '/api/backups' && method === 'GET') {
            const cid = url.searchParams.get('containerId') || undefined;
            const backups = this.backupManager.listBackups(cid);
            return jsonResponse(backups);
        }
        if (pathname === '/api/backups' && method === 'POST') {
            const body = await parseBody();
            const containerDir = this.containerManager.getContainerDir(body.containerId);
            const backup = await this.backupManager.createBackup(body.containerId, containerDir, body.name);
            return jsonResponse(backup, 201);
        }
        const backupDelMatch = pathname.match(/^\/api\/backups\/([a-zA-Z0-9_-]+)$/);
        if (backupDelMatch && method === 'DELETE') {
            const id = backupDelMatch[1];
            const ok = this.backupManager.deleteBackup(id);
            return jsonResponse({ success: ok });
        }
        return jsonResponse({ error: 'Endpoint not found' }, 404);
    }
    handleStaticRoute(_req, res, pathname) {
        let cleanPath = pathname === '/' ? 'index.html' : pathname.replace(/^\/+/, '');
        cleanPath = path.normalize(cleanPath).replace(/^(\.\.[\/\\])+/, '');
        const filePath = path.join(this.uiDir, cleanPath);
        if (!fs.existsSync(filePath) || !fs.statSync(filePath).isFile()) {
            // Fallback to index.html for SPA routing
            const indexPath = path.join(this.uiDir, 'index.html');
            if (fs.existsSync(indexPath)) {
                return this.serveFile(res, indexPath, 'text/html; charset=utf-8');
            }
            res.writeHead(404, { 'Content-Type': 'text/plain' });
            res.end('Not Found');
            return;
        }
        const mimeTypes = {
            '.html': 'text/html; charset=utf-8',
            '.css': 'text/css; charset=utf-8',
            '.js': 'application/javascript; charset=utf-8',
            '.json': 'application/json; charset=utf-8',
            '.png': 'image/png',
            '.jpg': 'image/jpeg',
            '.jpeg': 'image/jpeg',
            '.svg': 'image/svg+xml',
            '.ico': 'image/x-icon',
            '.woff2': 'font/woff2'
        };
        const ext = path.extname(filePath).toLowerCase();
        const contentType = mimeTypes[ext] || 'application/octet-stream';
        this.serveFile(res, filePath, contentType);
    }
    serveFile(res, filePath, contentType) {
        try {
            const content = fs.readFileSync(filePath);
            res.writeHead(200, {
                'Content-Type': contentType,
                'Content-Length': content.length,
                'Cache-Control': 'no-cache'
            });
            res.end(content);
        }
        catch {
            res.writeHead(500, { 'Content-Type': 'text/plain' });
            res.end('Internal Server Error');
        }
    }
}
