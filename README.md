# Aether Launcher

[![Build & Tests](https://img.shields.io/badge/tests-15%20passed-success)](https://github.com/Sqikpeek/Aether)
[![Language](https://img.shields.io/badge/language-C%2B%2B%20%2F%20Win32-blue)](https://github.com/Sqikpeek/Aether)
[![Binary Size](https://img.shields.io/badge/binary-~240%20KB-brightgreen)](https://github.com/Sqikpeek/Aether)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

Pure native C++ Win32 Minecraft launcher with zero Chrome/Edge browser dependencies, instant startup (< 20 ms), prominent in-dock nickname selection, automatic Mojang CDN asset downloads, and crash diagnostics.

[Русский](#russian) | [English](#english)

---

## Russian

Aether Launcher — нативный, легковесный и высокопроизводительный лаунчер Minecraft нового поколения, написанный на **чистом C++ (Win32 API)** без зависимостей от Chrome, Edge, WebView2 или Electron.

### Ключевые возможности:
- **100% чистый C++ (Win32 API)**: Никакого Chrome или Node.js для работы GUI. Вес приложения всего ~240 КБ, мгновенный запуск.
- **Никнейм в главном меню**: Поле ввода ника доступно прямо на нижней панели главного экрана перед кнопкой «Играть».
- **Надежный запуск игры**:
  - Автоматический поиск Java (8, 16, 17, 21+).
  - Автоматическая загрузка официального `client.jar` с Mojang CDN при первом запуске выбранной версии.
  - Полноценное формирование classpath, аргументов JVM и запуск `net.minecraft.client.main.Main`.
  - Потоковый перехват логов игры и встроенная диагностика причин крашей.
- **Мониторинг серверов**: Встроенный SLP-пинг серверов через сокеты Winsock.
- **Нативный установщик**: `Aether Installer.exe` для автоматического развертывания и создания ярлыков.

### Ссылки:
- Релизы: https://github.com/Sqikpeek/Aether/releases
- Лаунчер: `Aether.exe`
- Установщик: `Aether Installer.exe`
- Пакет обновления: `Aether Update.zip`

---

## English

Aether Launcher is a 100% pure native C++ Win32 Minecraft launcher engineered for maximum performance, zero bloat, and zero browser dependencies (no Chrome, Edge, WebView2, or Electron required).

### Key Features:
- **Pure Native C++ Win32 Desktop App**: Executable footprint is only ~240 KB with instant startup (< 20 ms) and ~15 MB RAM usage.
- **Nickname on Main Launch Dock**: Enter your player nickname directly on the primary launch screen without diving into settings.
- **Robust Minecraft Launch Workflow**:
  - Auto-discovery of installed Java JRE/JDKs across system registry and paths.
  - Automatic Mojang CDN client download (`piston-meta.mojang.com`) when launching a version for the first time.
  - Full classpath assembly, JVM argument configuration, and async process lifecycle management via `CreateProcessW`.
  - Real-time game log streaming with intelligent crash diagnostics.
- **Native Server Pinger**: Winsock-based Server List Ping (SLP) for real-time latency, status, and player counts.
- **Native Installer**: Compact `Aether Installer.exe` wizard for automatic deployment and shortcut creation.

### Links:
- Releases: https://github.com/Sqikpeek/Aether/releases
- Launcher: `Aether.exe`
- Installer: `Aether Installer.exe`
- Update Package: `Aether Update.zip`
