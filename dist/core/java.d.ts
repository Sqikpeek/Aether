export interface JavaInstallation {
    path: string;
    majorVersion: number;
    fullVersion: string;
    vendor: string;
    is64Bit: boolean;
    isValid: boolean;
}
export declare class JavaDetector {
    /**
     * Scans the system for installed Java runtimes.
     */
    static detectJavaInstallations(): JavaInstallation[];
    /**
     * Inspects a specific java executable path and extracts its version, vendor, and architecture.
     */
    static inspectJavaExecutable(javaPath: string): JavaInstallation | null;
    /**
     * Parses the text output of `java -version`.
     */
    static parseJavaVersionOutput(javaPath: string, output: string): JavaInstallation;
    /**
     * Returns required Java major version for a given Minecraft release version.
     */
    static getRequiredJavaVersion(minecraftVersion: string): number;
    /**
     * Selects the most compatible installed Java version for the container.
     */
    static findBestJavaForMinecraft(minecraftVersion: string, installations: JavaInstallation[]): JavaInstallation | null;
    private static findWindowsJavaPaths;
    private static findLinuxJavaPaths;
    private static findMacJavaPaths;
}
