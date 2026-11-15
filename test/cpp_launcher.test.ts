import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';

test('Native C++ Windows launcher and installer executables exist and are valid PE binaries', () => {
  const launcherPath = path.resolve('Aether.exe');
  const installerPath = path.resolve('Aether Installer.exe');

  assert.ok(fs.existsSync(launcherPath), 'Aether.exe must exist in root');
  assert.ok(fs.existsSync(installerPath), 'Aether Installer.exe must exist in root');

  const launcherBuffer = fs.readFileSync(launcherPath);
  const installerBuffer = fs.readFileSync(installerPath);

  // Check MZ header
  assert.equal(launcherBuffer[0], 0x4D, 'Aether.exe must start with MZ header (M)');
  assert.equal(launcherBuffer[1], 0x5A, 'Aether.exe must start with MZ header (Z)');

  assert.equal(installerBuffer[0], 0x4D, 'Aether Installer.exe must start with MZ header (M)');
  assert.equal(installerBuffer[1], 0x5A, 'Aether Installer.exe must start with MZ header (Z)');

  // Verify PE signature offset
  const launcherPeOffset = launcherBuffer.readUInt32LE(0x3C);
  assert.equal(launcherBuffer.toString('utf8', launcherPeOffset, launcherPeOffset + 4), 'PE\0\0');

  const installerPeOffset = installerBuffer.readUInt32LE(0x3C);
  assert.equal(installerBuffer.toString('utf8', installerPeOffset, installerPeOffset + 4), 'PE\0\0');

  // Verify Machine type is x86_64 (0x8664)
  const launcherMachine = launcherBuffer.readUInt16LE(launcherPeOffset + 4);
  assert.equal(launcherMachine, 0x8664, 'Aether.exe must be compiled for x86_64');

  const installerMachine = installerBuffer.readUInt16LE(installerPeOffset + 4);
  assert.equal(installerMachine, 0x8664, 'Aether Installer.exe must be compiled for x86_64');
});

test('Native C++ source files are present and contain core Minecraft launch logic', () => {
  const launcherMain = fs.readFileSync('src/cpp/launcher_main.cpp', 'utf8');
  const mcLauncher = fs.readFileSync('src/cpp/minecraft_launcher.cpp', 'utf8');
  const configHpp = fs.readFileSync('src/cpp/config.hpp', 'utf8');

  // Verify nickname is on main screen
  assert.ok(launcherMain.includes('НИКНЕЙМ') || launcherMain.includes('Никнейм'), 'Launcher UI must have Nickname label on main screen');
  assert.ok(launcherMain.includes('g_hEditNick') && launcherMain.includes('IDC_EDIT_NICKNAME'), 'Launcher UI must have nickname input control');
  assert.ok(launcherMain.includes('g_hoverPlayBtn') || launcherMain.includes('g_hBtnPlay') || launcherMain.includes('ИГРАТЬ'), 'Launcher UI must have Play button');

  // Verify Minecraft launch logic
  assert.ok(mcLauncher.includes('CreateProcessW'), 'Minecraft launcher must use CreateProcessW');
  assert.ok(mcLauncher.includes('net.minecraft.client.main.Main'), 'Minecraft launcher must reference official main class');
  assert.ok(mcLauncher.includes('InternetOpenUrlW') || mcLauncher.includes('InternetReadFile'), 'Downloader must support downloading client.jar via WinINet');
  assert.ok(mcLauncher.includes('AnalyzeCrash'), 'Crash analyzer must be present');

  // Verify Config
  assert.ok(configHpp.includes('AetherLauncher'), 'Config directory must be %APPDATA%\\AetherLauncher');
  assert.ok(configHpp.includes('LoadConfig'), 'Config loader must be present');
});
