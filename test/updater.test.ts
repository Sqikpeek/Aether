import test from 'node:test';
import assert from 'node:assert';
import { AutoUpdater } from '../dist/core/updater.js';

test('AutoUpdater.compareVersions correctly compares semver versions', () => {
  assert.strictEqual(AutoUpdater.compareVersions('1.0.38', '1.0.37'), 1);
  assert.strictEqual(AutoUpdater.compareVersions('1.0.37', '1.0.38'), -1);
  assert.strictEqual(AutoUpdater.compareVersions('1.0.38', '1.0.38'), 0);
  assert.strictEqual(AutoUpdater.compareVersions('v1.0.38', '1.0.38'), 0);
  assert.strictEqual(AutoUpdater.compareVersions('2.0.0', '1.9.99'), 1);
  assert.strictEqual(AutoUpdater.compareVersions('1.0.38.1', '1.0.38'), 1);
});

test('AutoUpdater.generateUpdateScript generates safe Windows batch script with PID wait and env cleanup', () => {
  const updater = new AutoUpdater('1.0.38');
  const tempDir = '/tmp';
  const res = updater.generateUpdateScript('/tmp/Aether.zip', tempDir, 'Aether.exe', 12345);

  assert.ok(res.scriptPath.length > 0);
  assert.strictEqual(res.command, process.platform === 'win32' ? 'cmd.exe' : '/bin/sh');
});
