import fs from 'node:fs';
import path from 'node:path';
import { spawn } from 'node:child_process';
import { EventEmitter } from 'node:events';
import https from 'node:https';
import { getSystemCa } from './http_utils.js';
import { buildJvmArguments } from './jvm_presets.js';
import { JavaDetector } from './java.js';
import { CrashAnalyzer } from './crash_analyzer.js';
export class GameLauncher extends EventEmitter {
    currentState = 'idle';
    activeProcess = null;
    activeContainerId = '';
    launchStartTime = 0;
    logBuffer = [];
    maxLogLines = 5000;
    logStream = null;
    mojangVersionsCache = [];
    constructor() {
        super();
    }
    getState() {
        return this.currentState;
    }
    getActiveContainerId() {
        return this.activeContainerId;
    }
    getRecentLogs() {
        return [...this.logBuffer];
    }
    clearLogs() {
        this.logBuffer = [];
        this.emit('logs_cleared');
    }
    /**
     * Fetches official list of verified Minecraft release versions from Mojang.
     */
    async getAvailableMinecraftVersions() {
        if (this.mojangVersionsCache.length > 0) {
            return this.mojangVersionsCache;
        }
        try {
            const data = await this.fetchJson('https://piston-meta.mojang.com/mc/game/version_manifest_v2.json');
            if (data && Array.isArray(data.versions)) {
                const releases = data.versions
                    .filter((v) => v.type === 'release')
                    .map((v) => v.id);
                this.mojangVersionsCache = releases;
                return releases;
            }
        }
        catch {
            // Fallback offline curated list
        }
        return [
            '1.21.3', '1.21.2', '1.21.1', '1.21',
            '1.20.6', '1.20.5', '1.20.4', '1.20.2', '1.20.1', '1.20',
            '1.19.4', '1.19.3', '1.19.2', '1.19',
            '1.18.2', '1.18.1', '1.18',
            '1.17.1', '1.17',
            '1.16.5', '1.16.4', '1.15.2', '1.14.4', '1.12.2', '1.7.10'
        ];
    }
    /**
     * Launches Minecraft for the specified container.
     * Event-driven with 0.00% background CPU usage while running.
     */
    async launch(options) {
        if (this.currentState === 'running' || this.currentState === 'preparing') {
            throw new Error('A game session is already active or starting');
        }
        this.setState('preparing');
        this.activeContainerId = options.container.id;
        this.launchStartTime = Date.now();
        this.clearLogs();
        const logsDir = path.join(options.containerDir, 'logs');
        if (!fs.existsSync(logsDir)) {
            fs.mkdirSync(logsDir, { recursive: true });
        }
        const latestLogFile = path.join(logsDir, 'latest.log');
        this.logStream = fs.createWriteStream(latestLogFile, { flags: 'w' });
        this.addLog('INFO', `[Aether Launcher] Initializing launch for container: ${options.container.name}`);
        this.addLog('INFO', `[Aether Launcher] Minecraft Version: ${options.container.minecraftVersion}, Loader: ${options.container.loaderType}`);
        // Step 1: Java Runtime Resolution
        let javaExe = options.javaPath;
        let javaInfo = null;
        if (!javaExe) {
            this.addLog('INFO', '[Aether Launcher] Auto-detecting compatible Java installation...');
            const installed = JavaDetector.detectJavaInstallations();
            javaInfo = JavaDetector.findBestJavaForMinecraft(options.container.minecraftVersion, installed);
            if (javaInfo) {
                javaExe = javaInfo.path;
                this.addLog('INFO', `[Aether Launcher] Selected Java ${javaInfo.majorVersion} (${javaInfo.vendor}) at ${javaExe}`);
            }
        }
        else {
            javaInfo = JavaDetector.inspectJavaExecutable(javaExe);
        }
        if (!javaExe || !fs.existsSync(javaExe)) {
            // Check if system has 'java' command in PATH
            javaExe = process.platform === 'win32' ? 'java.exe' : 'java';
            this.addLog('WARN', `[Aether Launcher] Specific Java not found, falling back to system command: ${javaExe}`);
        }
        // Step 2: Validate Java version compatibility
        const requiredJava = JavaDetector.getRequiredJavaVersion(options.container.minecraftVersion);
        if (javaInfo && javaInfo.majorVersion > 0) {
            if (javaInfo.majorVersion < requiredJava) {
                const warnMsg = `[Aether Launcher] WARNING: Minecraft ${options.container.minecraftVersion} requires Java ${requiredJava}+, but detected Java is ${javaInfo.majorVersion}. The game may crash on startup!`;
                this.addLog('WARN', warnMsg);
            }
        }
        // Step 3: Build JVM & Game Arguments
        const memory = options.container.memoryMax || options.allocatedMemoryMb || 4096;
        const jvmArgs = buildJvmArguments(memory, options.jvmPreset || 'aikar', options.container.jvmArgsOverride || options.customJvmArgs || '', javaInfo?.majorVersion || 17);
        // Prepare dummy / basic game classpath & args
        // In full launcher environment, jars are in <storage>/libraries and <storage>/versions
        const gameArgs = this.buildGameArguments(options);
        const fullArgs = [...jvmArgs, ...gameArgs];
        this.addLog('INFO', `[Aether Launcher] Launch command arguments prepared (${fullArgs.length} args, Memory: ${memory}MB)`);
        // Step 4: Spawn Game Process
        try {
            this.activeProcess = spawn(javaExe, fullArgs, {
                cwd: options.containerDir,
                detached: false,
                windowsHide: true,
                stdio: ['pipe', 'pipe', 'pipe']
            });
            this.setState('running');
            this.emit('game_started', {
                pid: this.activeProcess.pid,
                containerId: options.container.id
            });
            this.addLog('INFO', `[Aether Launcher] Game process spawned successfully (PID: ${this.activeProcess.pid})`);
            // Event-driven stdout/stderr (Zero CPU polling!)
            this.activeProcess.stdout?.on('data', (chunk) => {
                this.handleProcessOutput(chunk.toString('utf-8'), 'INFO');
            });
            this.activeProcess.stderr?.on('data', (chunk) => {
                this.handleProcessOutput(chunk.toString('utf-8'), 'ERROR');
            });
            this.activeProcess.on('error', (err) => {
                this.addLog('FATAL', `[Aether Launcher] Process failed to execute: ${err.message}`);
                this.handleProcessExit(1);
            });
            this.activeProcess.on('exit', (code, signal) => {
                const exitCode = code !== null ? code : 0;
                this.addLog('INFO', `[Aether Launcher] Game process exited with code ${exitCode} (Signal: ${signal || 'none'})`);
                this.handleProcessExit(exitCode);
            });
        }
        catch (err) {
            this.setState('idle');
            this.addLog('FATAL', `[Aether Launcher] Failed to spawn process: ${err.message}`);
            throw err;
        }
    }
    /**
     * Gracefully or forcefully stops running game process.
     */
    stopGame() {
        if (!this.activeProcess) {
            this.setState('idle');
            return;
        }
        this.setState('stopping');
        this.addLog('INFO', '[Aether Launcher] Terminating game process...');
        try {
            if (process.platform === 'win32') {
                spawn('taskkill', ['/F', '/T', '/PID', this.activeProcess.pid.toString()], {
                    windowsHide: true,
                    stdio: 'ignore'
                });
            }
            else {
                this.activeProcess.kill('SIGTERM');
                setTimeout(() => {
                    if (this.activeProcess && !this.activeProcess.killed) {
                        this.activeProcess.kill('SIGKILL');
                    }
                }, 3000);
            }
        }
        catch (err) {
            this.addLog('ERROR', `[Aether Launcher] Error stopping process: ${err.message}`);
        }
    }
    handleProcessOutput(text, defaultLevel) {
        if (this.logStream) {
            this.logStream.write(text);
        }
        const lines = text.split('\n');
        for (const line of lines) {
            const trimmed = line.trim();
            if (!trimmed)
                continue;
            let level = defaultLevel;
            if (/\[FATAL\]|FATAL/i.test(trimmed))
                level = 'FATAL';
            else if (/\[ERROR\]|ERROR|Exception|Error:/i.test(trimmed))
                level = 'ERROR';
            else if (/\[WARN\]|WARNING|WARN/i.test(trimmed))
                level = 'WARN';
            else if (/\[DEBUG\]|DEBUG/i.test(trimmed))
                level = 'DEBUG';
            this.addLog(level, trimmed);
        }
    }
    handleProcessExit(exitCode) {
        if (this.logStream) {
            this.logStream.end();
            this.logStream = null;
        }
        const durationSeconds = Math.round((Date.now() - this.launchStartTime) / 1000);
        this.activeProcess = null;
        if (exitCode !== 0) {
            this.setState('crashed');
            // Run diagnostic on logs
            const fullLogText = this.logBuffer.map(l => l.message).join('\n');
            const diagnostic = CrashAnalyzer.analyzeLog(fullLogText);
            this.emit('game_crashed', {
                exitCode,
                durationSeconds,
                diagnostic,
                containerId: this.activeContainerId
            });
        }
        else {
            this.setState('idle');
            this.emit('game_stopped', {
                durationSeconds,
                containerId: this.activeContainerId
            });
        }
        this.activeContainerId = '';
    }
    buildGameArguments(options) {
        const username = options.username || 'Player';
        const uuid = '00000000-0000-0000-0000-000000000000';
        const version = options.container.minecraftVersion;
        return [
            `-Djava.library.path=${path.join(options.containerDir, 'natives')}`,
            `-Dminecraft.launcher.brand=Aether`,
            `-Dminecraft.launcher.version=1.0.38`,
            `net.minecraft.client.main.Main`,
            `--username`, username,
            `--version`, version,
            `--gameDir`, options.containerDir,
            `--assetsDir`, path.join(options.storageDir, 'assets'),
            `--assetIndex`, version,
            `--uuid`, uuid,
            `--accessToken`, '0',
            `--userType`, 'mojang',
            `--versionType`, `Aether-${options.container.loaderType}`,
            `--width`, (options.resolution?.width || 925).toString(),
            `--height`, (options.resolution?.height || 530).toString()
        ];
    }
    addLog(level, message) {
        const entry = {
            timestamp: new Date().toLocaleTimeString(),
            level,
            message
        };
        this.logBuffer.push(entry);
        if (this.logBuffer.length > this.maxLogLines) {
            this.logBuffer.shift();
        }
        this.emit('log_entry', entry);
    }
    setState(state) {
        this.currentState = state;
        this.emit('state_changed', state);
    }
    fetchJson(url) {
        return new Promise((resolve, reject) => {
            const ca = getSystemCa();
            https
                .get(url, { ca, headers: { 'User-Agent': 'Aether-Launcher' } }, res => {
                let data = '';
                res.setEncoding('utf-8');
                res.on('data', chunk => (data += chunk));
                res.on('end', () => {
                    try {
                        resolve(JSON.parse(data));
                    }
                    catch (err) {
                        reject(err);
                    }
                });
            })
                .on('error', reject);
        });
    }
}
