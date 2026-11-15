import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { execSync } from 'node:child_process';

console.log('[Package] Starting packaging for Windows release assets...');

const releaseDir = path.resolve('release');
if (!fs.existsSync(releaseDir)) {
  fs.mkdirSync(releaseDir, { recursive: true });
}

// Step 1: Compile Native C++ Windows Executables
console.log('[Package] Compiling native C++ Windows PE binaries...');
try {
  execSync('zig c++ -target x86_64-windows -Wl,--subsystem,windows -Os src/cpp/launcher_main.cpp src/cpp/minecraft_launcher.cpp -lgdiplus -lgdi32 -lcomctl32 -lwininet -lws2_32 -lshlwapi -lshell32 -o Aether.exe', { stdio: 'inherit' });
  execSync('zig c++ -target x86_64-windows -Wl,--subsystem,windows -Os src/cpp/installer_main.cpp -lshell32 -o "Aether Installer.exe"', { stdio: 'inherit' });
  fs.copyFileSync('Aether Installer.exe', 'release/Aether Installer.exe');
  console.log('[Package] Successfully compiled native C++ Aether.exe and Aether Installer.exe');
} catch (err) {
  console.warn('[Package] Warning compiling native C++ binaries:', err.message);
}

// Step 2: Build update package (Aether Update.zip)
const stagingDir = path.resolve('.update-staging');
if (fs.existsSync(stagingDir)) {
  fs.rmSync(stagingDir, { recursive: true, force: true });
}
fs.mkdirSync(stagingDir, { recursive: true });

fs.copyFileSync('Aether.exe', path.join(stagingDir, 'Aether.exe'));
if (fs.existsSync('README_RU.md')) {
  fs.copyFileSync('README_RU.md', path.join(stagingDir, 'README.txt'));
}

const updateZipPath = path.join(releaseDir, 'Aether Update.zip');
if (fs.existsSync(updateZipPath)) {
  fs.unlinkSync(updateZipPath);
}

console.log(`[Package] Creating release zip: ${updateZipPath}...`);
execSync(`zip -j "${updateZipPath}" "${path.join(stagingDir, 'Aether.exe')}"`, { stdio: 'ignore' });

// Calculate SHA-256 and size
const zipBuffer = fs.readFileSync(updateZipPath);
const sha256 = crypto.createHash('sha256').update(zipBuffer).digest('hex').toLowerCase();
const size = zipBuffer.length;

console.log(`[Package] Archive size: ${size} bytes`);
console.log(`[Package] SHA-256: ${sha256}`);

// Also copy as Aether.zip for backward compatibility
fs.copyFileSync(updateZipPath, path.join(releaseDir, 'Aether.zip'));

// Clean staging
fs.rmSync(stagingDir, { recursive: true, force: true });

console.log('[Package] Packaging completed successfully!');
