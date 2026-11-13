import test from 'node:test';
import assert from 'node:assert';
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { BackupManager } from '../dist/core/backup.js';

test('BackupManager creates and manages container snapshots', async () => {
  const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), 'aether-test-backups-'));
  try {
    const manager = new BackupManager(tmpDir);

    // Setup dummy container folder
    const containerDir = path.join(tmpDir, 'test_container');
    const savesDir = path.join(containerDir, 'saves', 'world1');
    const configDir = path.join(containerDir, 'config');
    fs.mkdirSync(savesDir, { recursive: true });
    fs.mkdirSync(configDir, { recursive: true });

    fs.writeFileSync(path.join(savesDir, 'level.dat'), 'dummy world data');
    fs.writeFileSync(path.join(configDir, 'options.txt'), 'gamma:1.0');

    // Create backup
    const backup = await manager.createBackup('test_container', containerDir, 'Pre-Modpack Snapshot');

    assert.ok(backup.id.startsWith('backup_test_container_'));
    assert.strictEqual(backup.name, 'Pre-Modpack Snapshot');
    assert.ok(backup.sizeBytes > 0);
    assert.ok(fs.existsSync(backup.filePath));

    // List backups
    const list = manager.listBackups('test_container');
    assert.strictEqual(list.length, 1);
    assert.strictEqual(list[0].id, backup.id);

    // Delete backup
    const deleted = manager.deleteBackup(backup.id);
    assert.strictEqual(deleted, true);
    assert.strictEqual(manager.listBackups('test_container').length, 0);
  } finally {
    fs.rmSync(tmpDir, { recursive: true, force: true });
  }
});
