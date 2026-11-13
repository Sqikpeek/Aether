import { JvmPreset } from '../config.js';
export interface JvmPresetDefinition {
    id: JvmPreset;
    name: string;
    name_ru: string;
    description: string;
    description_ru: string;
    flags: string[];
}
export declare const JVM_PRESETS: Record<JvmPreset, JvmPresetDefinition>;
export declare function buildJvmArguments(memoryMb: number, preset?: JvmPreset, customArgs?: string, javaMajorVersion?: number): string[];
