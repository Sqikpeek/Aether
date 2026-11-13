import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { execSync } from 'node:child_process';

console.log('[Package] Starting packaging for Windows release assets...');

// Step 1: Build TypeScript and UI
execSync('node scripts/build.js', { stdio: 'inherit' });

const distDir = path.resolve('dist');
const releaseDir = path.resolve('release');

if (!fs.existsSync(releaseDir)) {
  fs.mkdirSync(releaseDir, { recursive: true });
}

// Step 2: Compile native Windows executables (Aether.exe and Aether Installer.exe)
console.log('[Package] Compiling native Windows PE binaries (Aether.exe and Aether Installer.exe)...');
try {
  execSync('zig cc -target x86_64-windows -Wl,--subsystem,windows -Os src/native/launcher.c -o dist/Aether.exe', { stdio: 'inherit' });
  execSync('zig cc -target x86_64-windows -Wl,--subsystem,windows -Os src/native/installer.c -o release/"Aether Installer.exe"', { stdio: 'inherit' });
  // Also keep at root
  fs.copyFileSync('dist/Aether.exe', 'Aether.exe');
  fs.copyFileSync('release/Aether Installer.exe', 'Aether Installer.exe');
  console.log('[Package] Successfully compiled Aether.exe and Aether Installer.exe');
} catch (err) {
  console.error('[Package] Failed compiling native executables:', err);
}

// Copy package.json to dist
fs.copyFileSync('package.json', path.join(distDir, 'package.json'));

// Helper to create zip archive of dist folder
function createZipFromDir(srcDir, zipPath) {
  try {
    execSync(`zip -r "${zipPath}" .`, { cwd: srcDir, stdio: 'ignore' });
  } catch {
    execSync(`tar -czf "${zipPath}" *`, { cwd: srcDir, stdio: 'ignore' });
  }
}

const updateZipPath = path.join(releaseDir, 'Aether Update.zip');
console.log(`[Package] Creating release zip: ${updateZipPath}...`);
createZipFromDir(distDir, updateZipPath);

// Calculate SHA-256 and size
const zipBuffer = fs.readFileSync(updateZipPath);
const sha256 = crypto.createHash('sha256').update(zipBuffer).digest('hex').toLowerCase();
const size = zipBuffer.length;

console.log(`[Package] Archive size: ${size} bytes`);
console.log(`[Package] SHA-256: ${sha256}`);

// Also copy as Aether.zip for backward compatibility
fs.copyFileSync(updateZipPath, path.join(releaseDir, 'Aether.zip'));

console.log('[Package] Packaging completed successfully!');
