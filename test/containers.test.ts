import test from 'node:test';
import assert from 'node:assert';
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { ContainerManager } from '../dist/core/containers.js';

test('ContainerManager handles container lifecycle and mod toggles', () => {
  const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), 'aether-test-containers-'));
  try {
    const manager = new ContainerManager(tmpDir);

    // Initial container created automatically
    const initialList = manager.listContainers();
    assert.ok(initialList.length >= 1);

    // Create container
    const c = manager.createContainer({
      name: 'Test Modded',
      minecraftVersion: '1.20.4',
      loaderType: 'fabric',
      shareSaves: false
    });

    assert.strictEqual(c.name, 'Test Modded');
    assert.strictEqual(c.minecraftVersion, '1.20.4');
    assert.strictEqual(c.loaderType, 'fabric');

    // Check retrieval
    const retrieved = manager.getContainer(c.id);
    assert.ok(retrieved !== null);
    assert.strictEqual(retrieved?.id, c.id);

    // Update container
    const updated = manager.updateContainer(c.id, { name: 'Renamed Modded' });
    assert.strictEqual(updated?.name, 'Renamed Modded');

    // Clone container
    const cloned = manager.cloneContainer(c.id, 'Cloned Modded');
    assert.ok(cloned !== null);
    assert.strictEqual(cloned?.name, 'Cloned Modded');

    // Test mod toggle
    const modsDir = manager.getContainerModsDir(c.id);
    const modFile = path.join(modsDir, 'sodium-fabric-0.5.8.jar');
    fs.writeFileSync(modFile, 'dummy jar content');

    const modsBefore = manager.listInstalledMods(c.id);
    assert.strictEqual(modsBefore.length, 1);
    assert.strictEqual(modsBefore[0].enabled, true);

    // Toggle off
    const toggled = manager.toggleMod(c.id, 'sodium-fabric-0.5.8.jar');
    assert.strictEqual(toggled, true);
    assert.ok(fs.existsSync(path.join(modsDir, 'sodium-fabric-0.5.8.jar.disabled')));

    const modsAfter = manager.listInstalledMods(c.id);
    assert.strictEqual(modsAfter[0].enabled, false);

    // Toggle back on
    const toggledOn = manager.toggleMod(c.id, 'sodium-fabric-0.5.8.jar.disabled');
    assert.strictEqual(toggledOn, true);
    assert.ok(fs.existsSync(modFile));

    // Delete mod
    const deletedMod = manager.deleteMod(c.id, 'sodium-fabric-0.5.8.jar');
    assert.strictEqual(deletedMod, true);
    assert.strictEqual(manager.listInstalledMods(c.id).length, 0);

    // Delete container
    const deleted = manager.deleteContainer(c.id);
    assert.strictEqual(deleted, true);
    assert.strictEqual(manager.getContainer(c.id), null);
  } finally {
    fs.rmSync(tmpDir, { recursive: true, force: true });
  }
});
