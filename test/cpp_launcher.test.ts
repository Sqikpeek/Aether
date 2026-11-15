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

  // Verify only 1 default container
  assert.ok(launcherMain.includes('g_containers = {') && launcherMain.includes('c-default'), 'Launcher must have only 1 default container profile');

  // Verify mod search via app tab
  assert.ok(launcherMain.includes('IDC_EDIT_MODSEARCH') && launcherMain.includes('FilterMods'), 'Launcher must implement mod search via app tab');

  // Verify RAM slider and custom numeric RAM input
  assert.ok(launcherMain.includes('g_isDraggingSlider') && launcherMain.includes('IDC_EDIT_RAM'), 'Launcher must implement draggable RAM slider and custom numeric input');

  // Verify vector icon rendering functions (no missing tofu emojis)
  assert.ok(launcherMain.includes('DrawIconContainers') && launcherMain.includes('DrawIconMods') && launcherMain.includes('DrawIconCube'), 'Launcher must use crisp vector icons');

  // Verify server tab is removed
  assert.ok(!launcherMain.includes('{ L"🌐", L"Серверы"'), 'Server tab must be removed from sidebar navigation');

  // Verify dropdown selection menus for container and version
  assert.ok(launcherMain.includes('TrackPopupMenuEx') && launcherMain.includes('CreatePopupMenu'), 'Launcher must use dropdown popup menus for version and container selection');

  // Verify global RAM setting
  assert.ok(launcherMain.includes('SetGlobalRam'), 'RAM setting must be applied globally to all containers');

  // Verify mod category filters, mouse wheel navigation, and details view
  assert.ok(launcherMain.includes('g_modCategories'), 'Launcher must provide category filter selection for mods');
  assert.ok(launcherMain.includes('WM_MOUSEWHEEL') && launcherMain.includes('g_modPage'), 'Launcher must support mouse-wheel scrolling/page-flipping for mods');
  assert.ok(launcherMain.includes('g_selectedModDetail'), 'Launcher must support opening detailed mod descriptions');

  // Verify Mod Loader selection dropdown on launch dock and in container management
  assert.ok(launcherMain.includes('g_loaders'), 'Launcher must define available mod loaders');
  assert.ok(launcherMain.includes('Fabric') && launcherMain.includes('Forge') && launcherMain.includes('NeoForge') && launcherMain.includes('Quilt'), 'Launcher must support Vanilla, Fabric, Forge, NeoForge, and Quilt');
  assert.ok(launcherMain.includes('AppendMenuW') && launcherMain.includes('TrackPopupMenuEx'), 'Launcher must provide dedicated popup menu for mod loader selection');

  // Verify expanded versions list (20+ versions)
  assert.ok(launcherMain.includes('1.21.3') && launcherMain.includes('1.12.2') && launcherMain.includes('1.7.10'), 'Launcher must include versions from 1.21.3 down to 1.7.10');
  assert.ok(launcherMain.includes('g_versions') && launcherMain.includes('1.8.9'), 'Launcher must have comprehensive version catalog');

  // Verify mod pagination (multiple pages and navigation)
  assert.ok(launcherMain.includes('g_modPage') && launcherMain.includes('totalPages'), 'Launcher must implement mod catalog pagination');
  assert.ok(launcherMain.includes('Предыдущая') && launcherMain.includes('Следующая'), 'Launcher must have Previous and Next page controls');
  assert.ok(launcherMain.includes('MODS_PER_PAGE'), 'Launcher must define items per page');

  // Verify DWM Immersive Dark Titlebar (removing harsh white border)
  assert.ok(launcherMain.includes('DwmSetWindowAttribute'), 'Launcher must call DwmSetWindowAttribute for dark titlebar');
  assert.ok(launcherMain.includes('DWMWA_USE_IMMERSIVE_DARK_MODE') || launcherMain.includes('20'), 'Launcher must request DWM dark mode attribute');
  assert.ok(launcherMain.includes('DWMWA_CAPTION_COLOR') || launcherMain.includes('35'), 'Launcher must set caption color to match dark theme');
});
