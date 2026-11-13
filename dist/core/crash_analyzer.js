import fs from 'node:fs';
import path from 'node:path';
export class CrashAnalyzer {
    static analyzeLog(logText) {
        if (!logText || logText.trim().length === 0) {
            return {
                hasIssue: false,
                type: 'none',
                title: 'No crashes detected',
                title_ru: 'Сбоев не обнаружено',
                cause: 'The log is empty or game exited normally.',
                cause_ru: 'Лог пуст или игра завершилась нормально.',
                solution: '',
                solution_ru: ''
            };
        }
        // 1. UnsupportedClassVersionError (Java Version Mismatch)
        if (/UnsupportedClassVersionError/i.test(logText)) {
            const match = logText.match(/class file version (\d+\.\d+).*?(?:compiled with|this version of the Java Runtime only recognizes class file versions up to (\d+\.\d+))/i);
            let targetJava = 'a newer Java version';
            let currentJava = 'an older Java version';
            if (match) {
                const classVer = parseFloat(match[1]);
                if (classVer >= 65)
                    targetJava = 'Java 21+';
                else if (classVer >= 61)
                    targetJava = 'Java 17';
                else if (classVer >= 60)
                    targetJava = 'Java 16';
                else if (classVer >= 55)
                    targetJava = 'Java 11';
                else if (classVer >= 52)
                    targetJava = 'Java 8';
            }
            return {
                hasIssue: true,
                type: 'java_version_mismatch',
                title: 'Incompatible Java Runtime',
                title_ru: 'Несовместимая версия Java',
                cause: `The game or installed mods require ${targetJava}, but your current Java runtime is outdated.`,
                cause_ru: `Игра или установленные моды требуют ${targetJava}, но текущая версия Java устарела.`,
                solution: `In Launcher Settings or Container Settings, select or install ${targetJava} for this container.`,
                solution_ru: `В настройках лаунчера или контейнера выберите ${targetJava} для этого контейнера.`,
                logExcerpt: CrashAnalyzer.extractErrorLines(logText, 'UnsupportedClassVersionError', 5)
            };
        }
        // 2. OutOfMemoryError
        if (/OutOfMemoryError/i.test(logText) || /insufficient memory/i.test(logText)) {
            return {
                hasIssue: true,
                type: 'out_of_memory',
                title: 'Out of Memory (OOM)',
                title_ru: 'Недостаточно оперативной памяти',
                cause: 'The game exhausted its allocated RAM heap and was terminated by the JVM.',
                cause_ru: 'Игре не хватило выделенной оперативной памяти (RAM), и Java аварийно завершила процесс.',
                solution: 'Increase the allocated memory (RAM) slider in Container Settings or Launcher Settings (e.g. set to 4-6 GB).',
                solution_ru: 'Увеличьте ползунок выделенной памяти (RAM) в настройках лаунчера или контейнера (например, до 4-6 ГБ).',
                logExcerpt: CrashAnalyzer.extractErrorLines(logText, 'OutOfMemoryError', 4)
            };
        }
        // 3. Fabric / Loader Missing Dependencies
        if (/net\.fabricmc\.loader|FabricLoaderException|ModResolutionException|Missing.*mod|requires.*which is missing/i.test(logText) &&
            /requires/i.test(logText)) {
            const depMatch = logText.match(/Mod '([^']+)' requires version ([^ ]+) of '([^']+)'/i) ||
                logText.match(/requires.*?version\s+([^\s]+)\s+of\s+'?([a-zA-Z0-9_-]+)'?/i) ||
                logText.match(/requires.*?of\s+'?([a-zA-Z0-9_-]+)'?/i);
            let missingMod = 'dependency mod';
            if (depMatch) {
                missingMod = depMatch[3] || depMatch[2] || depMatch[1] || 'missing dependency';
            }
            return {
                hasIssue: true,
                type: 'missing_dependency',
                title: 'Missing Required Mod Dependency',
                title_ru: 'Отсутствует обязательная зависимость мода',
                cause: `A mod cannot load because it requires '${missingMod}' to be installed.`,
                cause_ru: `Мод не может запуститься, так как ему требуется установленный '${missingMod}'.`,
                solution: `Open the Mods / Modrinth tab, search for '${missingMod}' and install it into this container.`,
                solution_ru: `Перейдите на вкладку «Моды / Modrinth», найдите '${missingMod}' и установите его в этот контейнер.`,
                details: missingMod,
                logExcerpt: CrashAnalyzer.extractErrorLines(logText, 'requires', 4)
            };
        }
        // 4. Duplicate Mods
        if (/DuplicateModsFoundException|duplicate mod/i.test(logText)) {
            return {
                hasIssue: true,
                type: 'duplicate_mods',
                title: 'Duplicate Mod Files Found',
                title_ru: 'Обнаружены дубликаты модов',
                cause: 'Two different versions of the same mod were found in your mods folder.',
                cause_ru: 'В папке mods найдено несколько версий одного и того же мода.',
                solution: 'Open the Mods tab and delete or disable the older version of the duplicate mod.',
                solution_ru: 'Откройте вкладку «Моды» и удалите или отключите старую версию дублирующегося мода.',
                logExcerpt: CrashAnalyzer.extractErrorLines(logText, 'Duplicate', 4)
            };
        }
        // 5. OpenGL / GPU Driver Failure
        if (/GLFW error 65542|WGL: The driver does not appear to support OpenGL|Pixel format not accelerated/i.test(logText)) {
            return {
                hasIssue: true,
                type: 'gpu_opengl_error',
                title: 'Graphics Driver / OpenGL Error',
                title_ru: 'Ошибка видеокарты / драйвера OpenGL',
                cause: 'Your graphics card driver does not support required OpenGL functions.',
                cause_ru: 'Драйвер вашей видеокарты не поддерживает необходимые функции OpenGL.',
                solution: 'Update your graphics card drivers (NVIDIA, AMD, or Intel) from the manufacturer website.',
                solution_ru: 'Обновите драйверы видеокарты (NVIDIA, AMD или Intel) с официального сайта производителя.',
                logExcerpt: CrashAnalyzer.extractErrorLines(logText, 'GLFW error', 4)
            };
        }
        // 6. Mixin Crash / Mod conflict
        if (/org\.spongepowered\.asm\.mixin\.transformer\.throwables\.MixinTransformerError/i.test(logText)) {
            const modMatch = logText.match(/in class '([^']+)'/i) || logText.match(/plugin '([^']+)'/i);
            const modHint = modMatch ? modMatch[1] : 'a conflicting mod';
            return {
                hasIssue: true,
                type: 'mixin_conflict',
                title: 'Mod Incompatibility / Mixin Crash',
                title_ru: 'Конфликт модов / Ошибка Mixin',
                cause: `A mod caused a bytecode transformation conflict (${modHint}).`,
                cause_ru: `Один из модов вызвал конфликт внедрения кода Mixin (${modHint}).`,
                solution: 'Try disabling recently added mods one by one in the Mods tab to isolate the conflicting mod.',
                solution_ru: 'Попробуйте по очереди отключать недавно добавленные моды на вкладке «Моды», чтобы найти конфликтный.',
                details: modHint,
                logExcerpt: CrashAnalyzer.extractErrorLines(logText, 'MixinTransformerError', 5)
            };
        }
        // 7. General Crash
        if (/\[FATAL\]|\[ERROR\]|Exception in thread "main"|Crash report saved to/i.test(logText)) {
            return {
                hasIssue: true,
                type: 'generic_crash',
                title: 'Game Crashed with Errors',
                title_ru: 'Игра завершилась с ошибкой',
                cause: 'An unhandled exception occurred during game startup or execution.',
                cause_ru: 'Во время запуска или работы игры произошло необработанное исключение.',
                solution: 'Check the crash report and game logs in the Logs tab or copy them to share with mod authors.',
                solution_ru: 'Проверьте отчет об ошибке во вкладке «Логи» или скопируйте его для отправки разработчикам модов.',
                logExcerpt: CrashAnalyzer.extractErrorLines(logText, 'Exception', 6)
            };
        }
        return {
            hasIssue: false,
            type: 'none',
            title: 'Game Log Clean',
            title_ru: 'В логе нет критических ошибок',
            cause: 'No standard crash patterns found.',
            cause_ru: 'Стандартных признаков аварийного завершения не найдено.',
            solution: '',
            solution_ru: ''
        };
    }
    static analyzeContainerLogs(containerDir) {
        // Check crash-reports folder first
        const crashReportsDir = path.join(containerDir, 'crash-reports');
        if (fs.existsSync(crashReportsDir)) {
            try {
                const files = fs.readdirSync(crashReportsDir).filter(f => f.endsWith('.txt'));
                if (files.length > 0) {
                    // Sort newest first
                    files.sort((a, b) => {
                        const statA = fs.statSync(path.join(crashReportsDir, a));
                        const statB = fs.statSync(path.join(crashReportsDir, b));
                        return statB.mtimeMs - statA.mtimeMs;
                    });
                    const latestCrash = path.join(crashReportsDir, files[0]);
                    const content = fs.readFileSync(latestCrash, 'utf-8');
                    const diag = CrashAnalyzer.analyzeLog(content);
                    if (diag.hasIssue) {
                        diag.details = `Report: ${files[0]}`;
                        return diag;
                    }
                }
            }
            catch { }
        }
        // Check latest.log
        const latestLogPath = path.join(containerDir, 'logs', 'latest.log');
        if (fs.existsSync(latestLogPath)) {
            try {
                const logContent = fs.readFileSync(latestLogPath, 'utf-8');
                return CrashAnalyzer.analyzeLog(logContent);
            }
            catch { }
        }
        return {
            hasIssue: false,
            type: 'none',
            title: 'No logs available',
            title_ru: 'Логи недоступны',
            cause: 'Container has not run Minecraft yet.',
            cause_ru: 'Контейнер еще ни разу не запускался.',
            solution: '',
            solution_ru: ''
        };
    }
    static extractErrorLines(text, keyword, contextLines = 4) {
        const lines = text.split('\n');
        const idx = lines.findIndex(l => l.toLowerCase().includes(keyword.toLowerCase()));
        if (idx === -1)
            return '';
        const start = Math.max(0, idx - 1);
        const end = Math.min(lines.length, idx + contextLines);
        return lines.slice(start, end).join('\n').trim();
    }
}
