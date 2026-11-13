export interface CrashDiagnostic {
    hasIssue: boolean;
    type: string;
    title: string;
    title_ru: string;
    cause: string;
    cause_ru: string;
    solution: string;
    solution_ru: string;
    details?: string;
    logExcerpt?: string;
}
export declare class CrashAnalyzer {
    static analyzeLog(logText: string): CrashDiagnostic;
    static analyzeContainerLogs(containerDir: string): CrashDiagnostic;
    private static extractErrorLines;
}
