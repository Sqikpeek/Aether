# Aether Launcher

[![Build & Tests](https://img.shields.io/badge/tests-13%20passed-success)](https://github.com/Sqikpeek/Aether)
[![Version](https://img.shields.io/badge/version-1.0.38-blue)](https://github.com/Sqikpeek/Aether/releases)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

High-performance, ultra-low CPU Minecraft launcher with isolated containers, Modrinth integration, smart crash diagnostics, and resilient auto-updates.

[Русский](#russian) | [English](#english)

---

## Russian

Aether Launcher — современный, легковесный и высокопроизводительный лаунчер по Minecraft с практически нулевой нагрузкой на процессор (< 0.1% в простое, 0.00% во время игры), изолированными контейнерами, установкой контента с Modrinth, автообновлениями, мониторингом серверов и снимками миров.

### Возможности:
- Сверхбыстрый движок на Node.js / TypeScript (старт за 50 мс, размер < 1 МБ вместо 41 МБ).
- 0% нагрузки на ЦП в фоне и во время игры (событийное управление процессами).
- Отдельные контейнеры для Vanilla, Fabric, Quilt, Forge и NeoForge.
- Каталог Modrinth: установка модов, ресурспаков, датапаков в 1 клик.
- Быстрое переключение модов (`.jar` <-> `.jar.disabled`).
- Умная диагностика крашей с готовыми подсказками по исправлению ошибок.
- Встроенный пинг серверов (онлайн, пинг, игроки, MOTD).
- Снимки (бэкапы) миров и настроек в 1 клик.
- Поддержка тем оформления и аппаратно-ускоренный перелив кнопки «Играть».
- Мгновенное переключение языка (RU / EN).

### Ссылки:
- Релизы: https://github.com/Sqikpeek/Aether/releases
- Пакет обновления: `Aether Update.zip`
- Установщик: `Aether Installer.exe`

---

## English

Aether Launcher is a lightweight, high-performance next-generation Minecraft launcher featuring near-zero CPU load (< 0.1% idle, 0.00% during gameplay), isolated containers, direct Modrinth content installation, crash diagnostics, server monitoring, and robust auto-updates.

### Features:
- Blazing-fast Node.js / TypeScript engine (< 50 ms launch, < 1 MB package vs 41 MB).
- 0% background CPU usage via event-driven architecture.
- Independent containers for Vanilla, Fabric, Quilt, Forge, and NeoForge.
- Modrinth catalog: 1-click install for mods, datapacks, resource packs, and plugins.
- 1-click mod toggle (`.jar` <-> `.jar.disabled`).
- Smart crash diagnostics with automated cause and solution identification.
- Built-in server pinger (online status, latency, player count, MOTD).
- 1-click world and configuration snapshots (backups).
- 6 visual themes with GPU-accelerated theme-strict play button shimmer.
- Instant language switching (RU / EN).

### Links:
- Releases: https://github.com/Sqikpeek/Aether/releases
- Update package: `Aether Update.zip`
- Installer: `Aether Installer.exe`
