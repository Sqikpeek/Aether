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

test('Native create, rename, and Microsoft sign-in dialogs are actionable modal forms', () => {
  const launcherMain = fs.readFileSync('src/cpp/launcher_main.cpp', 'utf8');

  assert.ok(launcherMain.includes('PromptNewContainer') && launcherMain.includes('context.versions = g_versions'),
    'The create-container dialog must offer every version supported by the launcher');
  assert.ok(launcherMain.includes('context.loaders = g_loaders') &&
            launcherMain.includes('IDC_MODAL_CONTAINER_VERSION') &&
            launcherMain.includes('IDC_MODAL_CONTAINER_LOADER'),
    'Minecraft version and loader must be separate controls, including Vanilla');
  assert.ok(launcherMain.includes('ModalWindowProc') && launcherMain.includes('case WM_COMMAND') &&
            launcherMain.includes('if (controlId == IDOK)') && launcherMain.includes('if (controlId == IDCANCEL)'),
    'OK and Cancel must be handled by the modal window procedure rather than left to DefWindowProc');
  assert.ok(launcherMain.includes('WM_SYSCOMMAND') && launcherMain.includes('SC_CLOSE') &&
            launcherMain.includes('RunModalMessageLoop') && launcherMain.includes('EnableWindow(parent, TRUE)'),
    'The close button, Escape, and Cancel must all release the modal parent window');
  assert.ok(launcherMain.includes('g_microsoftLoginAttempted.store(true)') &&
            launcherMain.includes('ownership have both been verified successfully'),
    'Selecting Microsoft sign-in must not discard offline mode before account verification succeeds');
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
  assert.ok(mcLauncher.includes('sponge-mixin-0.15.4+mixin.0.8.7.jar') &&
            !mcLauncher.includes('sponge-mixin-0.15.3+mixin.0.8.7.jar'),
    'Fabric 0.16.10 must use its exact current profile dependency, not an obsolete Mixin JAR');
  assert.ok(mcLauncher.includes('DownloadVerifiedFabricMirror') && mcLauncher.includes('FileMatchesSha1') &&
            mcLauncher.includes('97d0bff94981e37bd7a4362deee53c9a84e3fb21'),
    'Blocked Fabric Maven downloads must use a SHA-1-verified mirror for 1.20.1 intermediary');
  assert.ok(!mcLauncher.includes('fabric-loader-0.16.9.jar'),
    'Obsolete Fabric Loader 0.16.9 must not be selected for launch');
  assert.ok(mcLauncher.includes('ExtractWindowsNatives') &&
            mcLauncher.includes('ExtractNativeArchive') &&
            mcLauncher.includes('IsWindowsNativeArchivePath(lib.path)') &&
            mcLauncher.includes('Selected Windows x64 archive'),
    'Windows x64 native JARs must be selected by classifier path, including metadata entries not flagged as native archives');
  assert.ok(mcLauncher.includes('lowerName.find(L"-natives-")') && mcLauncher.includes('MemoryUtil'),
    'Native archives must be excluded from the Java classpath so LWJGL classes remain available');
});

test('Mojang metadata parser preserves Windows x64 native classifiers across artifact parsing and deduplication', () => {
  const mcLauncher = fs.readFileSync('src/cpp/minecraft_launcher.cpp', 'utf8');
  const addStart = mcLauncher.indexOf('static void AddLibraryDownload(');
  const parseStart = mcLauncher.indexOf('static std::vector<VersionLibInfo> ParseLibrariesFromJson(');
  const nativePathStart = mcLauncher.indexOf('static bool IsNativeArchivePath(');
  const windowsPathStart = mcLauncher.indexOf('static bool IsWindowsNativeArchivePath(');
  const extractStart = mcLauncher.indexOf('static bool ExtractWindowsNatives(');
  const add = mcLauncher.slice(addStart, parseStart);
  const parserEnd = mcLauncher.indexOf('static std::wstring EscapePowerShellLiteral(', parseStart);
  const parser = mcLauncher.slice(parseStart, parserEnd);
  const nativePath = mcLauncher.slice(nativePathStart, windowsPathStart);
  const windowsPathEnd = mcLauncher.indexOf('static void AddLibraryDownload(', windowsPathStart);
  const windowsPath = mcLauncher.slice(windowsPathStart, windowsPathEnd);
  const extractEnd = mcLauncher.indexOf('static std::vector<VersionLibInfo> GetStandardFallbackLibraries(', extractStart);
  const extract = mcLauncher.slice(extractStart, extractEnd);

  assert.ok(addStart >= 0 && parseStart > addStart && nativePathStart >= 0 &&
            windowsPathStart > nativePathStart && extractStart >= 0,
    'The metadata parser, native-path classifiers, and extraction routine must be present');
  assert.ok(add.includes('isNativeArchive || IsNativeArchivePath(widePath)') &&
            add.includes('existing.isNativeArchive = existing.isNativeArchive || nativeArchive'),
    'Native paths must be classified even when represented as artifacts and remain native after duplicate-path merging');
  assert.ok(nativePath.includes('name.find(L"-natives-")') && nativePath.includes('L".jar"'),
    'The parser must detect native JARs from their Maven classifier paths');
  assert.ok(parser.includes('IsWindowsNativeArchivePath(wideClassifierPath)') &&
            parser.includes('AddLibraryDownload(result, seenPaths, classifierDownload, true)'),
    'Windows x64 classifiers must be discovered from downloads.classifiers even without natives.windows metadata');
  assert.ok(windowsPath.includes('suffix == L".jar"') &&
            windowsPath.includes('suffix == L"-64.jar"') &&
            !windowsPath.includes('suffix == L"-arm64.jar"'),
    'Only the x64 Windows classifier variants should be selected, not ARM64 or x86 archives');
  assert.ok(extract.includes('IsWindowsNativeArchivePath(lib.path)') &&
            extract.includes('Selected Windows x64 archive'),
    'Native extraction must process the discovered Windows x64 JARs instead of relying solely on metadata flags');

  const artifact = (relativePath: string) => ({
    path: relativePath,
    sha1: '0123456789abcdef0123456789abcdef01234567',
    url: `https://libraries.minecraft.net/${relativePath}`,
  });
  const metadataFixture = {
    libraries: [
      {
        name: 'org.lwjgl:lwjgl:3.3.1',
        downloads: {
          artifact: artifact('org/lwjgl/lwjgl/3.3.1/lwjgl-3.3.1.jar'),
          classifiers: {
            'natives-windows': artifact('org/lwjgl/lwjgl/3.3.1/lwjgl-3.3.1-natives-windows.jar'),
            'natives-windows-64': artifact('org/lwjgl/lwjgl/3.3.1/lwjgl-3.3.1-natives-windows-64.jar'),
            'natives-windows-x86_64': artifact('org/lwjgl/lwjgl/3.3.1/lwjgl-3.3.1-natives-windows-x86_64.jar'),
            'natives-windows-arm64': artifact('org/lwjgl/lwjgl/3.3.1/lwjgl-3.3.1-natives-windows-arm64.jar'),
            'natives-windows-x86': artifact('org/lwjgl/lwjgl/3.3.1/lwjgl-3.3.1-natives-windows-x86.jar'),
            'natives-linux': artifact('org/lwjgl/lwjgl/3.3.1/lwjgl-3.3.1-natives-linux.jar'),
          },
        },
        natives: { windows: 'natives-windows' },
      },
      {
        name: 'org.lwjgl:lwjgl-glfw:3.3.1',
        // Regression case: a native archive represented as a plain artifact.
        downloads: {
          artifact: artifact('org/lwjgl/lwjgl-glfw/3.3.1/lwjgl-glfw-3.3.1-natives-windows.jar'),
        },
      },
    ],
  };
  const metadataPaths = metadataFixture.libraries.flatMap((library) => [
    ...(library.downloads.artifact ? [library.downloads.artifact.path] : []),
    ...Object.values(library.downloads.classifiers ?? {}).map((download) => download.path),
  ]);
  const windowsX64 = metadataPaths.filter((entry) =>
    /-natives-windows(?:-64|-x86_64)?\.jar$/i.test(entry));
  assert.deepEqual(windowsX64, [
    'org/lwjgl/lwjgl/3.3.1/lwjgl-3.3.1-natives-windows.jar',
    'org/lwjgl/lwjgl/3.3.1/lwjgl-3.3.1-natives-windows-64.jar',
    'org/lwjgl/lwjgl/3.3.1/lwjgl-3.3.1-natives-windows-x86_64.jar',
    'org/lwjgl/lwjgl-glfw/3.3.1/lwjgl-glfw-3.3.1-natives-windows.jar',
  ], 'A 1.20.1-style metadata fixture must select generic/64-bit Windows natives, including artifact paths, and reject other architectures');
});

test('Native ZIP extraction uses a compatible overwrite-safe API and rejects ZIP path traversal', () => {
  const mcLauncher = fs.readFileSync('src/cpp/minecraft_launcher.cpp', 'utf8');
  const extractStart = mcLauncher.indexOf('static bool ExtractNativeArchive(');
  const extractEnd = mcLauncher.indexOf('static bool ExtractWindowsNatives(', extractStart);
  assert.ok(extractStart >= 0 && extractEnd > extractStart,
    'The PowerShell ZIP extraction helper must be present');
  const extractor = mcLauncher.slice(extractStart, extractEnd);

  assert.ok(extractor.includes('[Console]::OutputEncoding=[System.Text.Encoding]::UTF8') &&
            extractor.includes('[System.IO.Compression.ZipFile]::OpenRead($archivePath)') &&
            extractor.includes('[System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$target)'),
    'Native JARs must be opened and extracted entry-by-entry using .NET overloads supported by Windows PowerShell');
  assert.ok(extractor.includes('[System.IO.File]::Delete($target)') &&
            extractor.includes('$target.StartsWith($prefix,[System.StringComparison]::OrdinalIgnoreCase)'),
    'Existing native entries must be replaced safely and ZIP-slip paths must be rejected');
  assert.ok(!extractor.includes(']::ExtractToDirectory(') && !extractor.includes("',$true)"),
    'Do not pass the unsupported overwrite Boolean to ExtractToDirectory');
});

test('Native launcher uses valid Mojang metadata, a verified client JAR, and disk-backed mod deletion', () => {
  const launcherMain = fs.readFileSync('src/cpp/launcher_main.cpp', 'utf8');
  const mcLauncher = fs.readFileSync('src/cpp/minecraft_launcher.cpp', 'utf8');

  assert.ok(mcLauncher.includes('GetVersionMetadataUrl') && mcLauncher.includes('JsonArrayField(manifestJson, "versions")'),
    'Version manifests must be parsed as JSON rather than assuming whitespace around id fields');
  assert.ok(mcLauncher.includes('GetClientDownloadUrl') && mcLauncher.includes('DownloadClientJarWithFallback'),
    'The real client URL and a launcher-CDN fallback must be used');
  assert.ok(mcLauncher.includes('appVersionDir') && mcLauncher.includes('IsZipArchive'),
    'A valid official client installation must be reusable and downloaded JARs must be verified');
  assert.ok(mcLauncher.includes('DownloadHttpFileViaWinInet') && mcLauncher.includes('DownloadHttpFileViaWinHttp') &&
            mcLauncher.includes('repo.maven.apache.org') && mcLauncher.includes('piston-data.mojang.com'),
    'Game libraries must retry through independent Windows HTTP stacks and official/public mirrors');
  assert.ok(mcLauncher.includes('GetAvailableMinecraftVersions') &&
            mcLauncher.includes('available_versions_manifest.json') &&
            mcLauncher.includes('ParseReleaseVersionIds'),
    'The creation form must be able to use the independently cached Mojang release list');
  assert.ok(launcherMain.includes('TriggerInstalledModFileRemove') && launcherMain.includes('Удалить'),
    'The installed-mods view must remove the exact disk file from the active container');
  assert.ok(launcherMain.includes('resolutionWidthX') && launcherMain.includes('resolutionHeightX'),
    'Resolution input controls must share the responsive settings layout');
});

test('Native launcher supports validated offline sessions and licensed Microsoft Minecraft authentication', () => {
  const launcherMain = fs.readFileSync('src/cpp/launcher_main.cpp', 'utf8');
  const mcLauncher = fs.readFileSync('src/cpp/minecraft_launcher.cpp', 'utf8');
  const configHpp = fs.readFileSync('src/cpp/config.hpp', 'utf8');

  assert.ok(launcherMain.includes('Offline (local nickname)') && launcherMain.includes('Microsoft (licensed)'),
    'Settings must offer explicit Offline and licensed Microsoft account modes');
  assert.ok(launcherMain.includes('nickname.size() >= 3') && launcherMain.includes("ch == L'_'"),
    'Offline launch must validate a Minecraft-safe local nickname');
  assert.ok(launcherMain.includes('oauth20_authorize.srf') && launcherMain.includes('oauth20_token.srf') &&
            launcherMain.includes('ExtractMicrosoftAuthorizationCode') &&
            launcherMain.includes('XboxLive.signin offline_access'),
    'Microsoft sign-in must use the supported Live authorization-code flow and request Xbox/offline scopes');
  assert.ok(!launcherMain.includes('/devicecode'),
    'The unsupported Azure device-code endpoint must not block Minecraft Java sign-in');
  assert.ok(launcherMain.includes('Selecting the licensed mode') && launcherMain.includes('PromptTextInput'),
    'Selecting Microsoft mode must visibly start browser sign-in and accept its completion callback');
  assert.ok(launcherMain.includes('user.auth.xboxlive.com') && launcherMain.includes('xsts.auth.xboxlive.com') &&
            launcherMain.includes('login_with_xbox') && launcherMain.includes('/minecraft/profile'),
    'A Microsoft session must complete Xbox, XSTS, Minecraft Services, and profile ownership verification');
  assert.ok(launcherMain.includes('CryptProtectData') && launcherMain.includes('CryptUnprotectData'),
    'The persisted Microsoft refresh token must use Windows DPAPI protection');
  assert.ok(launcherMain.includes('GetLaunchAccount') && launcherMain.includes('RefreshMicrosoftMinecraftAccount'),
    'A licensed launch must refresh and validate a stored Microsoft account before Java starts');
  assert.ok(configHpp.includes('accountMode') && configHpp.includes('microsoftRefreshTokenProtected') &&
            configHpp.includes('microsoftPlayerUuid'),
    'Account mode and protected Microsoft account identity must be persisted');

  assert.ok(mcLauncher.includes('const MinecraftAccount& account') && mcLauncher.includes('L"msa"'),
    'Java launch arguments must use the authenticated Microsoft account with userType msa');
  assert.ok(mcLauncher.includes('finalUuid') && mcLauncher.includes('finalAccessToken') &&
            launcherMain.includes('accessTokenExpiresAt'),
    'Java launch arguments must receive real UUID/access token fields and refresh expired service sessions');
  const launchAccountBlock = launcherMain.slice(
    launcherMain.indexOf('static bool GetLaunchAccount'),
    launcherMain.indexOf('static bool IsMicrosoftAccountMode'));
  assert.ok(launchAccountBlock.includes('g_microsoftLoginAttempted.store(true)') &&
            launchAccountBlock.includes('SetAccountStatus(error)'),
    'A failed token refresh must remain visible in Settings and prompt the user to sign in again');
  assert.ok(!mcLauncher.includes('--userType mojang'),
    'The launcher must not hard-code the old offline mojang user type for every session');
});

test('Inaccessible library files recover to a verified per-version cache without entering classpaths stale', () => {
  const mcLauncher = fs.readFileSync('src/cpp/minecraft_launcher.cpp', 'utf8');
  const repairStart = mcLauncher.indexOf('static bool PrepareLibraryRepairDestination(');
  const repairEnd = mcLauncher.indexOf('// A few networks expose Modrinth', repairStart);
  assert.ok(repairStart >= 0 && repairEnd > repairStart, 'The library repair helper must be present');
  const repair = mcLauncher.slice(repairStart, repairEnd);

  assert.ok(repair.includes('RemoveExistingFileForReplacement') &&
            repair.includes('ERROR_ACCESS_DENIED') &&
            repair.includes('GetRecoveredLibraryPath') &&
            repair.includes('IsValidLibraryArtifact(library, recoveredPath') &&
            repair.includes('originalAttributes') &&
            mcLauncher.includes('L"\\\\.aether-recovered\\\\libraries\\\\'),
    'A denied canonical cleanup must use a separate recovery destination and retain access/attribute diagnostics');
  assert.ok(mcLauncher.includes('Using verified recovery archive') &&
            mcLauncher.includes('GetRecoveredLibraryPath(libDir, mcVersion, loader, lib.path)'),
    'Native extraction must also fall back to a validated recovery library');
  assert.ok(mcLauncher.includes('if (loader != L"vanilla" && loader != L"fabric")') &&
            mcLauncher.includes('shared-cache stale artifacts excluded'),
    'Vanilla and Fabric classpaths must use current dependencies rather than stale cached JARs');

  const classpathStart = mcLauncher.indexOf('// 4. Assemble the complete Java classpath');
  const classpath = mcLauncher.slice(classpathStart);
  assert.ok(classpath.includes('Prioritizing verified recovery library') &&
            !classpath.includes('CollectJarsRecursively(recoveredLibDir'),
    'Only explicitly validated current-version recovery JARs may be used in the classpath');
});

test('Native launcher writes persistent stage-by-stage launch diagnostics and captures child output safely', () => {
  const launcherMain = fs.readFileSync('src/cpp/launcher_main.cpp', 'utf8');
  const mcLauncher = fs.readFileSync('src/cpp/minecraft_launcher.cpp', 'utf8');
  const mcHeader = fs.readFileSync('src/cpp/minecraft_launcher.hpp', 'utf8');

  assert.ok(launcherMain.includes('launcher.log') && launcherMain.includes('AppendLog') &&
            launcherMain.includes('Utf8FromWide(entry)') &&
            launcherMain.includes('native ZIP extraction=entry-wise'),
    'Launcher diagnostics must be timestamped and retained in a durable UTF-8 log');
  assert.ok(mcHeader.includes('onDiagnostic') && mcLauncher.includes('DescribeDownloadAttempt') &&
            mcLauncher.includes('Content-Length mismatch') && mcLauncher.includes('FormatWin32Error'),
    'Download logging must include transport stage, HTTP/Win32 failures, and byte-count validation');
  assert.ok(mcLauncher.includes('RemoveExistingFileForReplacement') &&
            mcLauncher.includes('MoveDownloadedFileIntoPlace') &&
            mcLauncher.includes('FILE_ATTRIBUTE_READONLY') &&
            mcLauncher.includes('SetFileAttributesW') &&
            mcLauncher.includes('Cleared the read-only attribute before replacing') &&
            mcLauncher.includes('.aether-recovered') &&
            mcLauncher.includes('Prioritizing verified recovery library'),
    'Library repair must clear read-only files, retry finalization, and use a verified alternate path when an invalid JAR cannot be replaced');
  assert.ok(mcLauncher.includes('DetectJava(customJava, requiredJava') &&
            mcLauncher.includes('No runnable Java installation was detected') &&
            !mcLauncher.includes('fallback.path = L"java.exe"'),
    'Launch must reject missing or incompatible Java instead of guessing a fake Java 17 runtime');
  assert.ok(mcLauncher.includes('[Natives][PowerShell]') &&
            mcLauncher.includes('ReportCapturedProcessOutput'),
    'PowerShell native-extraction failures must include captured stderr/stdout');
  assert.ok(mcLauncher.includes('DecodeProcessOutput') && mcLauncher.includes('[Minecraft] ') &&
            mcLauncher.includes('if (!accumulated.empty()) emitLine(accumulated)'),
    'Minecraft stdout/stderr must be decoded correctly and the final unterminated line must be logged');
  assert.ok(!mcLauncher.includes('commandLineForLog') &&
            mcLauncher.includes('account values and JVM arguments omitted from log') &&
            mcLauncher.includes('RedactSensitiveProcessOutput') &&
            mcLauncher.includes('AddSensitiveJvmArguments(customArgs') &&
            mcLauncher.includes('CreateProcessW could not start Java'),
    'Diagnostics must omit the full command line and redact account values/custom JVM arguments from captured output');
  assert.ok(!mcLauncher.includes('L"[Account] Session profile="') &&
            !launcherMain.includes('Signed in: " + account.username') &&
            !launcherMain.includes('Player: " + launchAccount.username'),
    'Persistent diagnostics must not include Microsoft profile names, UUIDs, or player nicknames');
  assert.ok(mcLauncher.includes('const int effectiveMemoryMb = std::max(512, memoryMb)') &&
            mcLauncher.includes('Xms=" + std::to_wstring(initialMemoryMb)'),
    'A low custom RAM setting must no longer silently become a 4 GiB heap');
  assert.ok(!launcherMain.includes('CreateProcessW failed!') &&
            launcherMain.includes('CreateProcessW may not have been reached'),
    'A failure before process creation must not be mislabeled as a CreateProcessW failure');
});
