# Aether Launcher

[![Build & Tests](https://img.shields.io/badge/tests-19%20passed-success)](https://github.com/Sqikpeek/Aether)
[![Language](https://img.shields.io/badge/language-C%2B%2B%20%2F%20Win32%20GDI%2B-blue)](https://github.com/Sqikpeek/Aether)
[![Binary Size](https://img.shields.io/badge/binary-~771%20KB-brightgreen)](https://github.com/Sqikpeek/Aether)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

Pure native C++ Win32 GDI+ Minecraft launcher with zero Chrome/Edge browser dependencies, vector anti-aliased icons, 60 FPS double-buffered rendering, in-app Modrinth search, custom RAM slider with numeric input, and prominent in-dock nickname selection.

[Русский](#russian) | [English](#english)

---

## Russian

Aether Launcher — нативный, легковесный и высокопроизводительный лаунчер Minecraft нового поколения, написанный на **чистом C++ (Win32 API + GDI+)** без зависимостей от Chrome, Edge, WebView2 или Electron.

### Ключевые возможности:
- **100% чистый C++ (Win32 GDI+)**: Никакого Chrome или Node.js. Вес приложения около 771 КБ, мгновенный запуск.
- **Четкие векторные иконки**: Без пустых квадратиков и проблем со шрифтами на любых версиях Windows.
- **Контейнеры с отдельными настройками**: Форма создания задаёт название, любую доступную официальную версию Minecraft и загрузчик отдельно; профили можно переименовывать.
- **Полный список релизов Mojang**: Версии загружаются из официального манифеста, а локальный список остаётся доступен без интернета.
- **Надёжные окна и вход Microsoft**: OK, Отмена, Enter, Escape и крестик закрывают диалог и применяют только подтверждённые изменения; Microsoft sign-in переключает аккаунт только после проверки.
- **Поиск модов прямо во вкладке приложения**: Мгновенная фильтрация по каталогу Modrinth в реальном времени.
- **Слайдер и кастомный ввод RAM**: Плавный ползунок памяти и поле точного ввода произвольного объема в МБ.
- **Никнейм в главном меню**: Поле ввода ника доступно прямо на нижней панели главного экрана перед кнопкой «Играть».
- **Надежный запуск игры**: Автоматический поиск Java, загрузка `client.jar` с Mojang CDN, формирование classpath и запуск `net.minecraft.client.main.Main`.
- **Нативный установщик**: `Aether Installer.exe` для автоматического развертывания и создания ярлыков.

### Ссылки:
- Релизы: https://github.com/Sqikpeek/Aether/releases
- Лаунчер: `Aether.exe`
- Установщик: `Aether Installer.exe`
- Пакет обновления: `Aether Update.zip`

---

## English

Aether Launcher is a 100% pure native C++ Win32 GDI+ Minecraft launcher engineered for maximum performance, zero bloat, and zero browser dependencies (no Chrome, Edge, WebView2, or Electron required).

### Key Features:
- **Pure Native C++ Win32 GDI+ Desktop App**: Executable footprint is about 771 KB, with instant startup and 60 FPS double-buffered rendering.
- **Crisp Vector Icons**: Drawn with native GDI+ paths, ensuring zero missing glyphs/tofu on all Windows versions.
- **Container Profiles**: Create and rename profiles with separate Minecraft-version and mod-loader controls.
- **Mojang Release Catalog**: Fetches official release versions and preserves an offline fallback.
- **Microsoft Account Linking**: Browser sign-in verifies Xbox Live and Minecraft ownership before enabling licensed mode.
- **In-App Modrinth Search**: Live mod filtering by title, category, and description directly inside the Mods tab.
- **RAM Slider & Custom Numeric Input**: Smooth draggable memory slider with direct numeric input box (MB/GB).
- **Nickname on Main Launch Dock**: Enter your player nickname directly on the primary launch screen without diving into settings.
- **Robust Minecraft Launch Engine**: Java auto-discovery, Mojang client and library downloads with alternate endpoints and archive/checksum validation, and async process management via `CreateProcessW`.

### Links:
- Releases: https://github.com/Sqikpeek/Aether/releases
- Launcher: `Aether.exe`
- Installer: `Aether Installer.exe`
- Update Package: `Aether Update.zip`
