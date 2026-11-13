import test from 'node:test';
import assert from 'node:assert';
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { ConfigManager } from '../dist/config.js';

test('ConfigManager creates defaults and persists updates atomically', () => {
  const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), 'aether-test-config-'));
  try {
    const manager = new ConfigManager(tmpDir);
    const cfg = manager.get();

    assert.strictEqual(cfg.version, '1.0.38');
    assert.strictEqual(cfg.theme, 'void-purple');
    assert.strictEqual(cfg.language, 'ru');
    assert.strictEqual(cfg.lowCpuMode, true);

    // Update config
    const updated = manager.update({
      theme: 'crimson',
      language: 'en',
      allocatedMemory: 8192
    });

    assert.strictEqual(updated.theme, 'crimson');
    assert.strictEqual(updated.language, 'en');
    assert.strictEqual(updated.allocatedMemory, 8192);

    // Reload from new instance
    const reloadedManager = new ConfigManager(tmpDir);
    const reloadedCfg = reloadedManager.get();
    assert.strictEqual(reloadedCfg.theme, 'crimson');
    assert.strictEqual(reloadedCfg.language, 'en');
    assert.strictEqual(reloadedCfg.allocatedMemory, 8192);
  } finally {
    fs.rmSync(tmpDir, { recursive: true, force: true });
  }
});
