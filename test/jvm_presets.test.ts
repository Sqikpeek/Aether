import test from 'node:test';
import assert from 'node:assert';
import { buildJvmArguments, JVM_PRESETS } from '../dist/core/jvm_presets.js';

test('buildJvmArguments constructs correct flags for Aikar preset', () => {
  const flags = buildJvmArguments(4096, 'aikar', '-Dcustom=true', 17);

  assert.ok(flags.includes('-Xmx4096M'));
  assert.ok(flags.includes('-Xms1024M'));
  assert.ok(flags.includes('-XX:+UseG1GC'));
  assert.ok(flags.includes('-XX:+ParallelRefProcEnabled'));
  assert.ok(flags.includes('-Dcustom=true'));
});

test('buildJvmArguments falls back from Generative ZGC when Java < 21 is used', () => {
  const flags = buildJvmArguments(4096, 'generative-zgc', '', 17);

  // Falls back to G1GC because Java 17 doesn't support Generational ZGC
  assert.ok(flags.includes('-XX:+UseG1GC'));
  assert.ok(!flags.includes('-XX:+ZGenerational'));
});
