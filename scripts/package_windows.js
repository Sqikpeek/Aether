import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import zlib from 'node:zlib';
import { execSync } from 'node:child_process';

console.log('[Package] Starting packaging for Windows release assets...');

// Step 1: Ensure project is built
execSync('node scripts/build.js', { stdio: 'inherit' });

const distDir = path.resolve('dist');
const releaseDir = path.resolve('release');

if (!fs.existsSync(releaseDir)) {
  fs.mkdirSync(releaseDir, { recursive: true });
}

// Create Windows launcher runner scripts
const vbsRunner = `Set WshShell = CreateObject("WScript.Shell")
strDir = CreateObject("Scripting.FileSystemObject").GetParentFolderName(WScript.ScriptFullName)
WshShell.CurrentDirectory = strDir

' Check if node is available or run bundled runner
strCmd = "node dist\\index.js"
WshShell.Run strCmd, 0, False
`;

const batRunner = `@echo off
chcp 65001 >nul
cd /d "%~dp0"
if exist "%SystemRoot%\\System32\\wscript.exe" (
    start "" wscript.exe //B "%~dp0Aether.vbs"
) else (
    start /B node dist\\index.js
)
exit
`;

fs.writeFileSync(path.join(distDir, 'Aether.vbs'), vbsRunner, 'utf-8');
fs.writeFileSync(path.join(distDir, 'Aether.bat'), batRunner, 'utf-8');

// Copy package.json to dist
fs.copyFileSync('package.json', path.join(distDir, 'package.json'));

// Create Windows Installer script
const installerBat = `@echo off
chcp 65001 >nul
title Aether Launcher Setup
echo ========================================================
echo               AETHER LAUNCHER INSTALLER                  
echo ========================================================
echo.

set "DEFAULT_DIR=%APPDATA%\\AetherLauncher"
set /p "INSTALL_DIR=Choose installation folder [%DEFAULT_DIR%]: "
if "%INSTALL_DIR%"=="" set "INSTALL_DIR=%DEFAULT_DIR%"

echo.
echo Installing Aether Launcher to: %INSTALL_DIR%
if not exist "%INSTALL_DIR%" mkdir "%INSTALL_DIR%"

echo Copying launcher files...
xcopy /E /I /Y "%~dp0*" "%INSTALL_DIR%" >nul

echo Creating desktop shortcut...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$ws = New-Object -ComObject WScript.Shell; $s = $ws.CreateShortcut([System.IO.Path]::Combine([Environment]::GetFolderPath('Desktop'), 'Aether Launcher.lnk')); $s.TargetPath = '%INSTALL_DIR%\\Aether.bat'; $s.WorkingDirectory = '%INSTALL_DIR%'; $s.Save()"

echo.
echo ========================================================
echo            Installation completed successfully!          
echo ========================================================
echo You can now launch Aether from your Desktop shortcut.
timeout /T 3 >nul
exit
`;

fs.writeFileSync(path.join(distDir, 'installer.bat'), installerBat, 'utf-8');

// Helper to create zip archive of dist folder
function createZipFromDir(srcDir, zipPath) {
  // Use zip or powershell or node archiver
  try {
    execSync(`zip -r "${zipPath}" .`, { cwd: srcDir, stdio: 'ignore' });
  } catch {
    // If 'zip' command is missing, use node simple pack
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

// Copy installer
const installerDest = path.join(releaseDir, 'Aether Installer.exe');
// For Windows compatibility, installer script is provided
fs.copyFileSync(path.join(distDir, 'installer.bat'), path.join(releaseDir, 'Aether_Installer.bat'));

console.log('[Package] Successfully created release packages in release/');
