import test from 'node:test';
import assert from 'node:assert';
import { JavaDetector } from '../dist/core/java.js';

test('JavaDetector.parseJavaVersionOutput parses Java 8, 17, and 21 across various vendors', () => {
  // Test Java 8 (Oracle / Temurin)
  const out8 = `java version "1.8.0_312"\nJava(TM) SE Runtime Environment (build 1.8.0_312-b07)\nJava HotSpot(TM) 64-Bit Server VM (build 25.312-b07, mixed mode)`;
  const parsed8 = JavaDetector.parseJavaVersionOutput('/dummy/path/java', out8);
  assert.strictEqual(parsed8.majorVersion, 8);
  assert.strictEqual(parsed8.fullVersion, '1.8.0_312');
  assert.strictEqual(parsed8.is64Bit, true);

  // Test Java 17 (Eclipse Temurin)
  const out17 = `openjdk version "17.0.10" 2024-01-16\nOpenJDK Runtime Environment Temurin-17.0.10+7 (build 17.0.10+7)\nOpenJDK 64-Bit Server VM Temurin-17.0.10+7 (build 17.0.10+7, mixed mode, sharing)`;
  const parsed17 = JavaDetector.parseJavaVersionOutput('/dummy/path/java17', out17);
  assert.strictEqual(parsed17.majorVersion, 17);
  assert.strictEqual(parsed17.vendor, 'Eclipse Temurin');
  assert.strictEqual(parsed17.is64Bit, true);

  // Test Java 21 (Microsoft OpenJDK)
  const out21 = `openjdk version "21.0.3" 2024-04-16 LTS\nOpenJDK Runtime Environment Microsoft-9288238 (build 21.0.3+9-LTS)\nOpenJDK 64-Bit Server VM Microsoft-9288238 (build 21.0.3+9-LTS, mixed mode, sharing)`;
  const parsed21 = JavaDetector.parseJavaVersionOutput('/dummy/path/java21', out21);
  assert.strictEqual(parsed21.majorVersion, 21);
  assert.strictEqual(parsed21.vendor, 'Microsoft OpenJDK');
  assert.strictEqual(parsed21.is64Bit, true);
});

test('JavaDetector.getRequiredJavaVersion matches Minecraft versions correctly', () => {
  assert.strictEqual(JavaDetector.getRequiredJavaVersion('1.12.2'), 8);
  assert.strictEqual(JavaDetector.getRequiredJavaVersion('1.16.5'), 8);
  assert.strictEqual(JavaDetector.getRequiredJavaVersion('1.17.1'), 16);
  assert.strictEqual(JavaDetector.getRequiredJavaVersion('1.18.2'), 17);
  assert.strictEqual(JavaDetector.getRequiredJavaVersion('1.20.1'), 17);
  assert.strictEqual(JavaDetector.getRequiredJavaVersion('1.20.4'), 17);
  assert.strictEqual(JavaDetector.getRequiredJavaVersion('1.20.5'), 21);
  assert.strictEqual(JavaDetector.getRequiredJavaVersion('1.21.1'), 21);
});
