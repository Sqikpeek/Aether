# Aether Launcher

**Aether Launcher** is a modern, lightweight, high-performance next-generation Minecraft launcher featuring near-zero CPU usage (< 0.1% idle, 0.00% during gameplay), isolated containers, direct Modrinth content installation, smart crash diagnostics, server status monitoring, and bulletproof auto-updates.

## Links

- **GitHub Repository**: https://github.com/Sqikpeek/Aether
- **Releases**: https://github.com/Sqikpeek/Aether/releases
- **Installer**: open the latest release and download `Aether Installer.exe`
- **Update Package**: releases provide `Aether Update.zip` for installation and auto-updates.

---

## Features

- 🚀 **Blazing-Fast Engine & 0% Background CPU**: rewritten from scratch using an event-driven asynchronous architecture. Starts in under 50 ms, eliminates heavy `tasklist` polling loops, and enters power-saving mode when idle.
- 📦 **Isolated Containers**: create independent profiles for different Minecraft versions, mod loaders, and custom modpacks.
- 📜 **Mojang Release Catalog**: refreshes the official release list, while keeping a local fallback for offline use; version and loader are separate choices.
- 🔐 **Microsoft Account Linking**: browser-based sign-in verifies Xbox Live and Minecraft ownership before enabling the licensed account mode.
- ⚡ **All Major Mod Loaders Supported**: Vanilla, Fabric, Quilt, Forge, and NeoForge.
- 🧩 **Modrinth v2 Integration**: search, browse, and install mods, datapacks, resource packs, and plugins in 1 click, automatically filtered by your container's version and loader.
- 🔄 **Instant Mod Toggle**: toggle installed mods ON and OFF (`.jar` <-> `.jar.disabled`) without deleting or re-downloading files.
- 💾 **Shared Saves**: share worlds between containers without data duplication (uses Directory Junctions on Windows and symbolic links on POSIX).
- ☕ **Smart Java Auto-Detection**: automatically detects installed Java runtimes (Adoptium, Oracle, Corretto, Zulu, Microsoft OpenJDK), checks bitness, and validates compatibility with Minecraft versions (Java 8 for 1.16.5, Java 17 for 1.18–1.20.4, Java 21+ for 1.20.5+).
- 🎮 **Resilient Game Downloads**: Mojang libraries are retried through alternate endpoints and validated as ZIP/JAR files against published SHA-1 checksums before launch.
- 🛠️ **Smart Crash Diagnostics**: automated log and crash report analyzer identifying Java version mismatches, Out of Memory errors, missing mod dependencies, and conflicts with clear step-by-step solutions.
- 📡 **Server Monitor & Ping**: check online status, latency (ms), player count, version, and formatted MOTD before launching the game.
- 📸 **World & Config Snapshots**: 1-click backups of saves and configuration files before updating or testing new mods.
- 🏎️ **Curated JVM Performance Presets**: Aikar's optimized G1GC flags (eliminates micro-stutters), Low RAM mode (for budget systems), and Generational ZGC for Java 21+.
- 🎨 **Visual Themes & Theme-Strict Play Button Shimmer**: 6 themes (Void Purple, Crimson Red, Cyberpunk Cyan, Emerald Green, Amber Gold, Nordic Blue). GPU-accelerated shimmer strictly obeys theme accent colors and pauses automatically to conserve CPU.
- 🌐 **Instant Language Switching**: seamless Russian and English support without reloading.
- 🔄 **Resilient Multi-Tier Auto-Update System**: fallback chain (GitHub API -> HTTP redirect -> raw manifest -> local cache) with streaming SHA-256 verification and safe atomic file replacement.

---

## Quick Start & Development

### Running from source
```bash
# Install dependencies
npm install

# Run automated tests
npm test

# Build the project
npm run build

# Start the launcher
npm start
```

### Development mode
```bash
npm run dev
```

The launcher interface will be accessible at `http://localhost:3000`.

---

## Release Assets

Each new GitHub Release contains two primary files:
- `Aether Installer.exe` — the installer.
- `Aether Update.zip` — the launcher package used for installation and auto updates.
