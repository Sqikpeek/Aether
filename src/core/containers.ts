import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { execSync } from 'node:child_process';

export type LoaderType = 'vanilla' | 'fabric' | 'quilt' | 'forge' | 'neoforge';

export interface ContainerConfig {
  id: string;
  name: string;
  minecraftVersion: string;
  loaderType: LoaderType;
  loaderVersion?: string;
  icon?: string;
  memoryMin?: number; // MB
  memoryMax?: number; // MB
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

export class ContainerManager {
  private storagePath: string;
  private containersDir: string;
  private sharedSavesDir: string;
  private metadataPath: string;
  private containers: Map<string, ContainerConfig> = new Map();

  constructor(storagePath: string) {
    this.storagePath = storagePath;
    this.containersDir = path.join(this.storagePath, 'containers');
    this.sharedSavesDir = path.join(this.storagePath, 'shared_saves');
    this.metadataPath = path.join(this.storagePath, 'containers.json');

    this.initDirectories();
    this.loadContainers();
  }

  private initDirectories(): void {
    if (!fs.existsSync(this.containersDir)) {
      fs.mkdirSync(this.containersDir, { recursive: true });
    }
    if (!fs.existsSync(this.sharedSavesDir)) {
      fs.mkdirSync(this.sharedSavesDir, { recursive: true });
    }
  }

  public listContainers(): ContainerConfig[] {
    return Array.from(this.containers.values()).sort((a, b) => (b.lastPlayed || 0) - (a.lastPlayed || 0));
  }

  public getContainer(id: string): ContainerConfig | null {
    return this.containers.get(id) || null;
  }

  public getContainerDir(id: string): string {
    return path.join(this.containersDir, id);
  }

  public getContainerModsDir(id: string): string {
    const dir = path.join(this.getContainerDir(id), 'mods');
    if (!fs.existsSync(dir)) {
      fs.mkdirSync(dir, { recursive: true });
    }
    return dir;
  }

  public getContainerSavesDir(id: string): string {
    return path.join(this.getContainerDir(id), 'saves');
  }

  public createContainer(params: {
    name: string;
    minecraftVersion: string;
    loaderType?: LoaderType;
    loaderVersion?: string;
    icon?: string;
    memoryMin?: number;
    memoryMax?: number;
    jvmArgsOverride?: string;
    shareSaves?: boolean;
  }): ContainerConfig {
    const id = `c_${Date.now()}_${crypto.randomBytes(3).toString('hex')}`;
    const config: ContainerConfig = {
      id,
      name: params.name.trim() || 'Minecraft',
      minecraftVersion: params.minecraftVersion.trim(),
      loaderType: params.loaderType || 'vanilla',
      loaderVersion: params.loaderVersion,
      icon: params.icon || 'cube',
      memoryMin: params.memoryMin,
      memoryMax: params.memoryMax,
      jvmArgsOverride: params.jvmArgsOverride,
      shareSaves: !!params.shareSaves,
      created: Date.now(),
      lastPlayed: 0,
      playTimeMinutes: 0
    };

    // Create container filesystem structure
    const containerDir = this.getContainerDir(id);
    fs.mkdirSync(containerDir, { recursive: true });
    fs.mkdirSync(path.join(containerDir, 'mods'), { recursive: true });
    fs.mkdirSync(path.join(containerDir, 'config'), { recursive: true });
    fs.mkdirSync(path.join(containerDir, 'resourcepacks'), { recursive: true });
    fs.mkdirSync(path.join(containerDir, 'shaderpacks'), { recursive: true });
    fs.mkdirSync(path.join(containerDir, 'logs'), { recursive: true });

    this.setupSavesLink(id, config.shareSaves);

    this.containers.set(id, config);
    this.saveContainers();
    return config;
  }

  public updateContainer(id: string, partial: Partial<ContainerConfig>): ContainerConfig | null {
    const existing = this.containers.get(id);
    if (!existing) return null;

    const prevShareSaves = existing.shareSaves;
    const updated: ContainerConfig = {
      ...existing,
      ...partial,
      id // preserve id
    };

    if (partial.shareSaves !== undefined && partial.shareSaves !== prevShareSaves) {
      this.setupSavesLink(id, updated.shareSaves);
    }

    this.containers.set(id, updated);
    this.saveContainers();
    return updated;
  }

  public cloneContainer(id: string, newName?: string): ContainerConfig | null {
    const src = this.containers.get(id);
    if (!src) return null;

    const cloneName = newName || `${src.name} (Copy)`;
    const newConfig = this.createContainer({
      name: cloneName,
      minecraftVersion: src.minecraftVersion,
      loaderType: src.loaderType,
      loaderVersion: src.loaderVersion,
      icon: src.icon,
      memoryMin: src.memoryMin,
      memoryMax: src.memoryMax,
      jvmArgsOverride: src.jvmArgsOverride,
      shareSaves: src.shareSaves
    });

    // Copy mods & configs
    const srcDir = this.getContainerDir(id);
    const destDir = this.getContainerDir(newConfig.id);

    try {
      this.copyDirRecursive(path.join(srcDir, 'mods'), path.join(destDir, 'mods'));
      this.copyDirRecursive(path.join(srcDir, 'config'), path.join(destDir, 'config'));
    } catch (err) {
      console.error(`[Containers] Error copying files during clone:`, err);
    }

    return newConfig;
  }

  public deleteContainer(id: string): boolean {
    if (!this.containers.has(id)) return false;

    const containerDir = this.getContainerDir(id);
    try {
      // Unlink saves directory if symlinked/junctioned
      const savesDir = path.join(containerDir, 'saves');
      if (fs.existsSync(savesDir)) {
        try {
          const stat = fs.lstatSync(savesDir);
          if (stat.isSymbolicLink()) {
            fs.unlinkSync(savesDir);
          }
        } catch {
          // If junction on Windows, rmdir works without deleting target content
        }
      }

      if (fs.existsSync(containerDir)) {
        fs.rmSync(containerDir, { recursive: true, force: true });
      }
    } catch (err) {
      console.error(`[Containers] Failed to delete directory for container ${id}:`, err);
    }

    this.containers.delete(id);
    this.saveContainers();
    return true;
  }

  public recordPlaySession(id: string, durationMinutes: number): void {
    const c = this.containers.get(id);
    if (c) {
      c.lastPlayed = Date.now();
      c.playTimeMinutes = (c.playTimeMinutes || 0) + durationMinutes;
      this.saveContainers();
    }
  }

  /**
   * Sets up or removes directory link for shared saves.
   * On Windows, uses Directory Junction (mklink /J) which does NOT need admin rights.
   * On POSIX, uses standard directory symlink.
   */
  public setupSavesLink(id: string, shareSaves: boolean): void {
    const savesDir = path.join(this.getContainerDir(id), 'saves');
    const sharedDir = this.sharedSavesDir;

    if (shareSaves) {
      if (fs.existsSync(savesDir)) {
        try {
          const stat = fs.lstatSync(savesDir);
          if (stat.isSymbolicLink()) {
            // Already symlinked
            return;
          }
          // If standard folder with worlds inside, copy them to shared_saves
          const worlds = fs.readdirSync(savesDir);
          for (const world of worlds) {
            const srcWorld = path.join(savesDir, world);
            const dstWorld = path.join(sharedDir, world);
            if (!fs.existsSync(dstWorld) && fs.statSync(srcWorld).isDirectory()) {
              this.copyDirRecursive(srcWorld, dstWorld);
            }
          }
          fs.rmSync(savesDir, { recursive: true, force: true });
        } catch (err) {
          console.warn(`[Containers] Error migrating saves for container ${id}:`, err);
        }
      }

      // Create link
      try {
        if (process.platform === 'win32') {
          // Directory junction
          execSync(`cmd.exe /c mklink /J "${savesDir}" "${sharedDir}"`, {
            windowsHide: true,
            stdio: 'ignore'
          });
        } else {
          fs.symlinkSync(sharedDir, savesDir, 'dir');
        }
      } catch (err) {
        console.warn(`[Containers] Could not create junction/symlink for shared saves:`, err);
        // Fallback: create normal folder
        if (!fs.existsSync(savesDir)) {
          fs.mkdirSync(savesDir, { recursive: true });
        }
      }
    } else {
      // Unlink shared saves
      if (fs.existsSync(savesDir)) {
        try {
          const stat = fs.lstatSync(savesDir);
          if (stat.isSymbolicLink()) {
            fs.unlinkSync(savesDir);
            fs.mkdirSync(savesDir, { recursive: true });
          } else if (process.platform === 'win32') {
            // Check if directory junction on Windows
            try {
              fs.rmdirSync(savesDir);
              fs.mkdirSync(savesDir, { recursive: true });
            } catch {
              // Standard folder, leave as is
            }
          }
        } catch {
          // Normal folder
        }
      } else {
        fs.mkdirSync(savesDir, { recursive: true });
      }
    }
  }

  /**
   * Scans container's mods folder and returns list of installed mods (both .jar and .jar.disabled).
   */
  public listInstalledMods(id: string): InstalledMod[] {
    const modsDir = this.getContainerModsDir(id);
    if (!fs.existsSync(modsDir)) return [];

    const files = fs.readdirSync(modsDir);
    const mods: InstalledMod[] = [];

    for (const f of files) {
      const isJar = f.endsWith('.jar');
      const isDisabled = f.endsWith('.jar.disabled');

      if (!isJar && !isDisabled) continue;

      const fullPath = path.join(modsDir, f);
      try {
        const stat = fs.statSync(fullPath);
        const baseName = f.replace(/\.jar(\.disabled)?$/, '');
        mods.push({
          id: baseName,
          fileName: f,
          name: this.formatModName(baseName),
          version: this.extractModVersion(baseName),
          enabled: isJar,
          fileSizeBytes: stat.size,
          modifiedTime: stat.mtimeMs
        });
      } catch {
        // Skip unreadable files
      }
    }

    return mods.sort((a, b) => a.name.localeCompare(b.name));
  }

  /**
   * Toggles a mod between enabled (.jar) and disabled (.jar.disabled) instantly without re-downloading.
   */
  public toggleMod(id: string, fileName: string): boolean {
    const modsDir = this.getContainerModsDir(id);
    const currentPath = path.join(modsDir, fileName);

    if (!fs.existsSync(currentPath)) return false;

    let targetName: string;
    if (fileName.endsWith('.jar')) {
      targetName = `${fileName}.disabled`;
    } else if (fileName.endsWith('.jar.disabled')) {
      targetName = fileName.replace(/\.disabled$/, '');
    } else {
      return false;
    }

    const targetPath = path.join(modsDir, targetName);
    try {
      fs.renameSync(currentPath, targetPath);
      return true;
    } catch (err) {
      console.error(`[Containers] Failed to toggle mod ${fileName}:`, err);
      return false;
    }
  }

  public deleteMod(id: string, fileName: string): boolean {
    const modsDir = this.getContainerModsDir(id);
    const fullPath = path.join(modsDir, fileName);
    if (fs.existsSync(fullPath)) {
      try {
        fs.unlinkSync(fullPath);
        return true;
      } catch {
        return false;
      }
    }
    return false;
  }

  // --- Helpers ---

  private formatModName(baseName: string): string {
    // E.g. "sodium-fabric-mc1.21.1-0.6.0" -> "Sodium Fabric"
    const cleaned = baseName
      .replace(/[-_](fabric|forge|neoforge|quilt)/gi, ' ')
      .replace(/[-_](mc)?\d+\.\d+(\.\d+)?/gi, ' ')
      .replace(/[-_]v?\d+\.\d+.*$/i, '')
      .replace(/[-_]+/g, ' ')
      .trim();

    if (!cleaned) return baseName;
    return cleaned
      .split(' ')
      .map(w => w.charAt(0).toUpperCase() + w.slice(1))
      .join(' ');
  }

  private extractModVersion(baseName: string): string {
    const match = baseName.match(/(\d+\.\d+[\w.-]*)$/);
    return match ? match[1] : '';
  }

  private copyDirRecursive(src: string, dest: string): void {
    if (!fs.existsSync(src)) return;
    fs.mkdirSync(dest, { recursive: true });
    const entries = fs.readdirSync(src, { withFileTypes: true });

    for (const entry of entries) {
      const srcPath = path.join(src, entry.name);
      const destPath = path.join(dest, entry.name);

      if (entry.isDirectory()) {
        this.copyDirRecursive(srcPath, destPath);
      } else {
        fs.copyFileSync(srcPath, destPath);
      }
    }
  }

  private loadContainers(): void {
    try {
      if (fs.existsSync(this.metadataPath)) {
        const raw = fs.readFileSync(this.metadataPath, 'utf-8');
        const list: ContainerConfig[] = JSON.parse(raw);
        this.containers.clear();
        for (const item of list) {
          this.containers.set(item.id, item);
        }
      }
    } catch (err) {
      console.warn(`[Containers] Failed to load ${this.metadataPath}:`, err);
    }

    // Default container if empty
    if (this.containers.size === 0) {
      this.createContainer({
        name: 'Default Vanilla 1.21.1',
        minecraftVersion: '1.21.1',
        loaderType: 'vanilla',
        shareSaves: true
      });
    }
  }

  private saveContainers(): void {
    try {
      const list = Array.from(this.containers.values());
      const tmpPath = `${this.metadataPath}.tmp.${Date.now()}`;
      fs.writeFileSync(tmpPath, JSON.stringify(list, null, 2), 'utf-8');
      fs.renameSync(tmpPath, this.metadataPath);
    } catch (err) {
      console.error(`[Containers] Failed to save containers to ${this.metadataPath}:`, err);
    }
  }
}
