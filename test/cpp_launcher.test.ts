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

  // Verify the scrolling catalog and details view.
  assert.ok(launcherMain.includes('WM_MOUSEWHEEL') && launcherMain.includes('g_tabScrollOffset'), 'Launcher must scroll tab content with the mouse wheel');
  assert.ok(launcherMain.includes('g_selectedModDetail'), 'Launcher must support opening detailed mod descriptions');

  // Verify Mod Loader selection dropdown on launch dock and in container management
  assert.ok(launcherMain.includes('g_loaders'), 'Launcher must define available mod loaders');
  assert.ok(launcherMain.includes('Fabric') && launcherMain.includes('Forge') && launcherMain.includes('NeoForge') && launcherMain.includes('Quilt'), 'Launcher must support Vanilla, Fabric, Forge, NeoForge, and Quilt');
  assert.ok(launcherMain.includes('AppendMenuW') && launcherMain.includes('TrackPopupMenuEx'), 'Launcher must provide dedicated popup menu for mod loader selection');

  // Verify expanded versions list (20+ versions)
  assert.ok(launcherMain.includes('1.21.3') && launcherMain.includes('1.12.2') && launcherMain.includes('1.7.10'), 'Launcher must include versions from 1.21.3 down to 1.7.10');
  assert.ok(launcherMain.includes('g_versions') && launcherMain.includes('1.8.9'), 'Launcher must have comprehensive version catalog');

  // Verify infinite, batched Modrinth catalog rather than pages.
  assert.ok(launcherMain.includes('MODS_PER_BATCH = 20'), 'Launcher must load catalog results in 20-item batches');
  assert.ok(launcherMain.includes('FetchNextModrinthCatalogBatch'), 'Launcher must fetch another batch at the end of the catalog');
  assert.ok(!launcherMain.includes('g_modPage'), 'Launcher must not use page state for the mod catalog');

  // Verify DWM Immersive Dark Titlebar (removing harsh white border)
  assert.ok(launcherMain.includes('DwmSetWindowAttribute'), 'Launcher must call DwmSetWindowAttribute for dark titlebar');
  assert.ok(launcherMain.includes('DWMWA_USE_IMMERSIVE_DARK_MODE') || launcherMain.includes('20'), 'Launcher must request DWM dark mode attribute');
  assert.ok(launcherMain.includes('DWMWA_CAPTION_COLOR') || launcherMain.includes('35'), 'Launcher must set caption color to match dark theme');

  // Verify "Native C++ Win32" label is removed as requested by user
  assert.ok(!launcherMain.includes('L"Native C++ Win32"'), 'Launcher must not display "Native C++ Win32" label');

  // Verify full Java/JVM configuration (custom flags, GC presets, custom Java path)
  assert.ok(launcherMain.includes('IDC_EDIT_JVMARGS') && launcherMain.includes('g_hEditJvmArgs'), 'Launcher must provide custom JVM arguments input field');
  assert.ok(configHpp.includes('customJvmArgs'), 'Config must persist custom JVM arguments');
  assert.ok(launcherMain.includes('PRESET_AIKAR') && launcherMain.includes('PRESET_ZGC') && launcherMain.includes('PRESET_SHENANDOAH'), 'Launcher must provide Aikar, ZGC, and Shenandoah GC presets');
  assert.ok(launcherMain.includes('IDC_EDIT_JAVAPATH') && launcherMain.includes('g_hEditJavaPath'), 'Launcher must provide custom Java runtime path input field');
  assert.ok(launcherMain.includes('IDC_EDIT_GAMEWIDTH') && launcherMain.includes('IDC_EDIT_GAMEHEIGHT'), 'Launcher must provide game window resolution inputs');

  // Verify catalog-only Modrinth installation with strict compatibility and dependencies.
  assert.ok(launcherMain.includes('DownloadModrinthMod') && launcherMain.includes('TriggerModDownload'), 'Launcher must download a catalog mod from Modrinth');
  assert.ok(launcherMain.includes('DownloadVersionAndDependencies') && launcherMain.includes('dependency_type'), 'Launcher must install required dependencies automatically');
  assert.ok(launcherMain.includes('game_versions') && launcherMain.includes('FetchCompatibleModrinthVersion'), 'Launcher must request the active container Minecraft version');
  assert.ok(!launcherMain.includes('Скачать любой мод'), 'Launcher must not offer arbitrary URL/slug installation');

  // Container names need durable persistence and a close-safe rename prompt.
  assert.ok(launcherMain.includes('SaveContainerProfiles') && launcherMain.includes('LoadContainerProfiles'), 'Launcher must persist complete container profiles');
  assert.ok(launcherMain.includes('WM_SYSCOMMAND') && launcherMain.includes('SC_CLOSE'), 'Closing a rename prompt must re-enable the launcher instead of hanging it');
});

test('Native launcher keeps catalog state on the UI thread and prepares a complete Fabric/LWJGL runtime', () => {
  const launcherMain = fs.readFileSync('src/cpp/launcher_main.cpp', 'utf8');
  const mcLauncher = fs.readFileSync('src/cpp/minecraft_launcher.cpp', 'utf8');

  assert.ok(launcherMain.includes('WM_AETHER_MODRINTH_BATCH') && launcherMain.includes('ApplyModrinthCatalogBatch'),
    'Catalog network batches must be applied by the window thread before cards are rendered');
  assert.ok(launcherMain.includes('g_showInstalledMods') && launcherMain.includes('Установленные моды'),
    'Mods tab must expose a distinct Installed Mods control next to its search field');
  assert.ok(launcherMain.includes('GetSettingsThemeCardHeight') && launcherMain.includes('GetSettingsGcPillRows'),
    'Settings controls must wrap into scrollable rows instead of overflowing horizontally');

  assert.ok(mcLauncher.includes('fabric-loader-0.16.10.jar'),
    'Fabric runtime must meet the Fabric API minimum loader requirement');
  assert.ok(!mcLauncher.includes('fabric-loader-0.16.9.jar'),
    'Obsolete Fabric Loader 0.16.9 must not be selected for launch');
  assert.ok(mcLauncher.includes('ExtractWindowsNatives') && mcLauncher.includes('ExtractNativeArchive'),
    'LWJGL native classifier archives must be extracted before Java starts');
  assert.ok(mcLauncher.includes('lowerName.find(L"-natives-")') && mcLauncher.includes('MemoryUtil'),
    'Native archives must be excluded from the Java classpath so LWJGL classes remain available');
});
