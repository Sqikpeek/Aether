import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';

export interface BackupItem {
  id: string;
  containerId: string;
  name: string;
  createdAt: number;
  sizeBytes: number;
  filePath: string;
}

export class BackupManager {
  private backupsDir: string;

  constructor(storagePath: string) {
    this.backupsDir = path.join(storagePath, 'backups');
    if (!fs.existsSync(this.backupsDir)) {
      fs.mkdirSync(this.backupsDir, { recursive: true });
    }
  }

  public listBackups(containerId?: string): BackupItem[] {
    if (!fs.existsSync(this.backupsDir)) return [];

    const files = fs.readdirSync(this.backupsDir);
    const result: BackupItem[] = [];

    for (const file of files) {
      if (!file.endsWith('.json.meta')) continue;

      try {
        const metaPath = path.join(this.backupsDir, file);
        const data: BackupItem = JSON.parse(fs.readFileSync(metaPath, 'utf-8'));

        if (!containerId || data.containerId === containerId) {
          if (fs.existsSync(data.filePath)) {
            result.push(data);
          }
        }
      } catch {}
    }

    return result.sort((a, b) => b.createdAt - a.createdAt);
  }

  /**
   * Creates a backup of container saves and config folders.
   */
  public async createBackup(
    containerId: string,
    containerDir: string,
    backupName?: string
  ): Promise<BackupItem> {
    const timestamp = Date.now();
    const id = `backup_${containerId}_${timestamp}`;
    const name = backupName || `Snapshot ${new Date(timestamp).toLocaleString()}`;
    const archivePath = path.join(this.backupsDir, `${id}.tar.gz`);

    // Collect files from saves and config
    const targetDirs = ['saves', 'config'];
    const filesToArchive: Array<{ relPath: string; fullPath: string }> = [];

    for (const dirName of targetDirs) {
      const fullDir = path.join(containerDir, dirName);
      if (fs.existsSync(fullDir)) {
        this.collectFilesRecursive(fullDir, dirName, filesToArchive);
      }
    }

    // Write simple tar-like archive compressed with gzip
    const outStream = fs.createWriteStream(archivePath);
    const gzip = zlib.createGzip();
    gzip.pipe(outStream);

    // Pack files: header [pathLen (2 bytes), path, size (8 bytes)], followed by file bytes
    for (const item of filesToArchive) {
      try {
        const stat = fs.statSync(item.fullPath);
        if (!stat.isFile()) continue;

        const pathBuf = Buffer.from(item.relPath, 'utf-8');
        const header = Buffer.alloc(2 + pathBuf.length + 8);
        header.writeUInt16BE(pathBuf.length, 0);
        pathBuf.copy(header, 2);
        header.writeBigUInt64BE(BigInt(stat.size), 2 + pathBuf.length);

        gzip.write(header);
        const content = fs.readFileSync(item.fullPath);
        gzip.write(content);
      } catch (err) {
        console.warn(`[Backup] Skipping file ${item.fullPath}:`, err);
      }
    }

    await new Promise<void>((resolve, reject) => {
      gzip.end();
      outStream.on('finish', () => resolve());
      outStream.on('error', reject);
    });

    const stat = fs.statSync(archivePath);
    const meta: BackupItem = {
      id,
      containerId,
      name,
      createdAt: timestamp,
      sizeBytes: stat.size,
      filePath: archivePath
    };

    const metaPath = path.join(this.backupsDir, `${id}.json.meta`);
    fs.writeFileSync(metaPath, JSON.stringify(meta, null, 2), 'utf-8');

    return meta;
  }

  public deleteBackup(id: string): boolean {
    const metaPath = path.join(this.backupsDir, `${id}.json.meta`);
    const archivePath = path.join(this.backupsDir, `${id}.tar.gz`);

    let ok = false;
    if (fs.existsSync(metaPath)) {
      fs.unlinkSync(metaPath);
      ok = true;
    }
    if (fs.existsSync(archivePath)) {
      fs.unlinkSync(archivePath);
      ok = true;
    }
    return ok;
  }

  private collectFilesRecursive(
    currentDir: string,
    relativeBase: string,
    list: Array<{ relPath: string; fullPath: string }>
  ): void {
    const entries = fs.readdirSync(currentDir, { withFileTypes: true });
    for (const e of entries) {
      const full = path.join(currentDir, e.name);
      const rel = path.join(relativeBase, e.name);
      if (e.isDirectory()) {
        this.collectFilesRecursive(full, rel, list);
      } else if (e.isFile()) {
        list.push({ relPath: rel, fullPath: full });
      }
    }
  }
}
