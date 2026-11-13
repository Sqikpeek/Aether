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

// Step 2: Compile native Windows executables (if compiler available)
console.log('[Package] Checking compiler for native Windows PE binaries...');
let hasZig = false;
try {
  execSync('zig version', { stdio: 'ignore' });
  hasZig = true;
} catch {}

if (hasZig) {
  try {
    console.log('[Package] Compiling native Windows PE binaries with embedded payload...');
    execSync('zig cc -target x86_64-windows -Wl,--subsystem,windows -Os src/native/launcher.c -o dist/Aether.exe', { stdio: 'inherit' });
    execSync('zig cc -target x86_64-windows -Wl,--subsystem,windows -Os src/native/installer.c -o release/"Aether Installer.exe"', { stdio: 'inherit' });
    fs.copyFileSync('dist/Aether.exe', 'Aether.exe');
    fs.copyFileSync('release/Aether Installer.exe', 'Aether Installer.exe');
    console.log('[Package] Successfully compiled standalone Aether.exe and Aether Installer.exe');
  } catch (err) {
    console.warn('[Package] Warning compiling native executables:', err.message);
  }
} else {
  console.log('[Package] Compiler not found; using existing precompiled Aether.exe and Aether Installer.exe');
  if (fs.existsSync('Aether.exe')) {
    fs.copyFileSync('Aether.exe', 'dist/Aether.exe');
  }
  if (fs.existsSync('Aether Installer.exe')) {
    fs.copyFileSync('Aether Installer.exe', 'release/Aether Installer.exe');
  }
}

// Copy package.json to dist
fs.copyFileSync('package.json', path.join(distDir, 'package.json'));

// Create update zip archive
function createZipFromDir(srcDir, zipPath) {
  if (fs.existsSync(zipPath)) {
    fs.unlinkSync(zipPath);
  }
  
  if (process.platform === 'win32') {
    try {
      execSync(`powershell -NoProfile -Command "Compress-Archive -Path '${srcDir}\\*' -DestinationPath '${zipPath}' -Force"`, { stdio: 'ignore' });
      return;
    } catch {}
  }

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
