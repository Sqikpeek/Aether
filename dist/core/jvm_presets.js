export const JVM_PRESETS = {
    aikar: {
        id: 'aikar',
        name: "Aikar's Optimized Flags",
        name_ru: "Оптимизированные флаги Aikar",
        description: "Industry standard G1GC flags eliminating GC pauses and frame drops.",
        description_ru: "Стандарт оптимизации G1GC, убирающий микрофризы и просадки FPS.",
        flags: [
            '-XX:+UseG1GC',
            '-XX:+ParallelRefProcEnabled',
            '-XX:MaxGCPauseMillis=200',
            '-XX:+UnlockExperimentalVMOptions',
            '-XX:+DisableExplicitGC',
            '-XX:+AlwaysPreTouch',
            '-XX:G1NewSizePercent=30',
            '-XX:G1MaxNewSizePercent=40',
            '-XX:G1ReservePercent=20',
            '-XX:G1HeapWastePercent=5',
            '-XX:G1MixedGCCountTarget=4',
            '-XX:InitiatingHeapOccupancyPercent=15',
            '-XX:G1MixedGCLiveThresholdPercent=90',
            '-XX:G1RSetUpdatingPauseTimePercent=5',
            '-XX:SurvivorRatio=32',
            '-XX:+PerfDisableSharedMem',
            '-XX:MaxTenuringThreshold=1'
        ]
    },
    'low-ram': {
        id: 'low-ram',
        name: 'Low RAM / Potato PC',
        name_ru: 'Экономия памяти (для слабых ПК)',
        description: 'Conservative memory footprint and lightweight GC settings.',
        description_ru: 'Минимальное потребление ОЗУ и легкая сборка мусора для слабых ПК.',
        flags: [
            '-XX:+UseSerialGC',
            '-XX:+AlwaysPreTouch',
            '-XX:MinHeapFreeRatio=10',
            '-XX:MaxHeapFreeRatio=20'
        ]
    },
    'generative-zgc': {
        id: 'generative-zgc',
        name: 'Generational ZGC (Java 21+)',
        name_ru: 'Generational ZGC (Java 21+)',
        description: 'Ultra-low pause time (< 1ms) garbage collector for modern Java 21+.',
        description_ru: 'Сверхбыстрый сборщик мусора с паузами менее 1 мс для Java 21+.',
        flags: [
            '-XX:+UseZGC',
            '-XX:+ZGenerational',
            '-XX:+AlwaysPreTouch',
            '-XX:+PerfDisableSharedMem'
        ]
    },
    standard: {
        id: 'standard',
        name: 'Standard Java Flags',
        name_ru: 'Стандартные флаги Java',
        description: 'Default HotSpot JVM arguments.',
        description_ru: 'Стандартные флаги сборщика HotSpot.',
        flags: [
            '-XX:+UseG1GC'
        ]
    },
    custom: {
        id: 'custom',
        name: 'Custom User Flags',
        name_ru: 'Пользовательские флаги',
        description: 'Uses only custom flags defined in settings.',
        description_ru: 'Используются только флаги, указанные пользователем.',
        flags: []
    }
};
export function buildJvmArguments(memoryMb, preset = 'aikar', customArgs = '', javaMajorVersion = 17) {
    const args = [];
    // Memory allocation
    const mem = Math.max(1024, memoryMb);
    args.push(`-Xmx${mem}M`);
    args.push(`-Xms${Math.min(1024, Math.floor(mem / 2))}M`);
    // Preset flags
    const presetDef = JVM_PRESETS[preset] || JVM_PRESETS.aikar;
    let flags = [...presetDef.flags];
    // If user selected ZGC on older Java < 21, fallback to G1GC gracefully
    if (preset === 'generative-zgc' && javaMajorVersion < 21) {
        flags = [...JVM_PRESETS.aikar.flags];
    }
    args.push(...flags);
    // Custom user args
    if (customArgs && customArgs.trim().length > 0) {
        const splitCustom = customArgs
            .trim()
            .split(/\s+/)
            .filter(a => a.length > 0);
        args.push(...splitCustom);
    }
    return args;
}
