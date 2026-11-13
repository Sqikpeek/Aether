import test from 'node:test';
import assert from 'node:assert';
import { CrashAnalyzer } from '../dist/core/crash_analyzer.js';

test('CrashAnalyzer detects Java version mismatch', () => {
  const log = `Exception in thread "main" java.lang.UnsupportedClassVersionError: net/minecraft/client/main/Main has been compiled by a more recent version of the Java Runtime (class file version 65.0), this version of the Java Runtime only recognizes class file versions up to 61.0`;
  const result = CrashAnalyzer.analyzeLog(log);

  assert.strictEqual(result.hasIssue, true);
  assert.strictEqual(result.type, 'java_version_mismatch');
  assert.ok(result.cause.includes('Java 21+'));
  assert.ok(result.solution.length > 0);
  assert.ok(result.solution_ru.length > 0);
});

test('CrashAnalyzer detects Out of Memory error', () => {
  const log = `[12:00:00] [main/FATAL]: Reported exception thrown!\njava.lang.OutOfMemoryError: Java heap space\n\tat java.base/java.util.Arrays.copyOf(Arrays.java:3512)`;
  const result = CrashAnalyzer.analyzeLog(log);

  assert.strictEqual(result.hasIssue, true);
  assert.strictEqual(result.type, 'out_of_memory');
  assert.ok(result.title.includes('Out of Memory'));
});

test('CrashAnalyzer detects Fabric missing dependency', () => {
  const log = `net.fabricmc.loader.impl.FormattedException: Some of your mods are incompatible with the game or each other!
A potential solution has been determined:
	 - Mod 'Sodium' requires version 0.90 or later of 'fabric-api', which is missing!`;
  const result = CrashAnalyzer.analyzeLog(log);

  assert.strictEqual(result.hasIssue, true);
  assert.strictEqual(result.type, 'missing_dependency');
  assert.ok(result.cause.includes('fabric-api'));
});
