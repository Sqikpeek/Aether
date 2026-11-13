import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';

export interface JavaInstallation {
  path: string;
  majorVersion: number;
  fullVersion: string;
  vendor: string;
  is64Bit: boolean;
  isValid: boolean;
}

export class JavaDetector {
  /**
   * Scans the system for installed Java runtimes.
   */
  public static detectJavaInstallations(): JavaInstallation[] {
    const candidates = new Set<string>();

    // 1. Check JAVA_HOME
    if (process.env.JAVA_HOME) {
      const binJava = path.join(
        process.env.JAVA_HOME,
        'bin',
        process.platform === 'win32' ? 'java.exe' : 'java'
      );
      candidates.add(binJava);
    }

    // 2. Check PATH
    const pathExt = process.env.PATH || '';
    const pathDirs = pathExt.split(path.delimiter);
    const exeName = process.platform === 'win32' ? 'java.exe' : 'java';

    for (const dir of pathDirs) {
      if (!dir) continue;
      const full = path.join(dir, exeName);
      if (fs.existsSync(full)) {
        candidates.add(full);
      }
    }

    // 3. Platform specific directories
    if (process.platform === 'win32') {
      JavaDetector.findWindowsJavaPaths(candidates);
    } else if (process.platform === 'darwin') {
      JavaDetector.findMacJavaPaths(candidates);
    } else {
      JavaDetector.findLinuxJavaPaths(candidates);
    }

    const results: JavaInstallation[] = [];
    const seenPaths = new Set<string>();

    for (const p of candidates) {
      const normalized = path.resolve(p);
      if (seenPaths.has(normalized)) continue;
      seenPaths.add(normalized);

      const info = JavaDetector.inspectJavaExecutable(normalized);
      if (info && info.isValid) {
        results.push(info);
      }
    }

    // Sort by major version descending
    return results.sort((a, b) => b.majorVersion - a.majorVersion);
  }

  /**
   * Inspects a specific java executable path and extracts its version, vendor, and architecture.
   */
  public static inspectJavaExecutable(javaPath: string): JavaInstallation | null {
    if (!fs.existsSync(javaPath)) return null;

    try {
      const proc = spawnSync(javaPath, ['-version'], {
        windowsHide: true,
        encoding: 'utf-8',
        timeout: 5000,
        stdio: ['ignore', 'pipe', 'pipe']
      });

      const output = `${proc.stdout || ''}\n${proc.stderr || ''}`;
      return JavaDetector.parseJavaVersionOutput(javaPath, output);
    } catch {
      return null;
    }
  }

  /**
   * Parses the text output of `java -version`.
   */
  public static parseJavaVersionOutput(javaPath: string, output: string): JavaInstallation {
    // Examples:
    // openjdk version "21.0.3" 2024-04-16 LTS
    // java version "1.8.0_312"
    // openjdk version "17.0.10" 2024-01-16
    let fullVersion = '';
    let majorVersion = 0;
    let vendor = 'OpenJDK';
    const is64Bit = /64-Bit|x86_64|amd64|aarch64|arm64/i.test(output);

    const versionMatch = output.match(/(?:java|openjdk)\s+version\s+"([^"]+)"/i);
    if (versionMatch && versionMatch[1]) {
      fullVersion = versionMatch[1];
      if (fullVersion.startsWith('1.')) {
        // E.g. "1.8.0_312" -> 8
        const sub = fullVersion.split('.')[1];
        majorVersion = parseInt(sub, 10) || 8;
      } else {
        // E.g. "21.0.3" -> 21
        const main = fullVersion.split('.')[0];
        majorVersion = parseInt(main, 10) || 0;
      }
    }

    // Detect vendor
    if (/Adoptium|Temurin/i.test(output)) vendor = 'Eclipse Temurin';
    else if (/Zulu/i.test(output)) vendor = 'Azul Zulu';
    else if (/Corretto/i.test(output)) vendor = 'Amazon Corretto';
    else if (/Microsoft/i.test(output)) vendor = 'Microsoft OpenJDK';
    else if (/Liberica|BellSoft/i.test(output)) vendor = 'BellSoft Liberica';
    else if (/Oracle/i.test(output)) vendor = 'Oracle Java';
    else if (/GraalVM/i.test(output)) vendor = 'GraalVM';

    return {
      path: javaPath,
      majorVersion,
      fullVersion,
      vendor,
      is64Bit,
      isValid: majorVersion > 0
    };
  }

  /**
   * Returns required Java major version for a given Minecraft release version.
   */
  public static getRequiredJavaVersion(minecraftVersion: string): number {
    const parts = minecraftVersion.split('.').map(p => parseInt(p, 10) || 0);
    const major = parts[0] || 1;
    const minor = parts[1] || 0;
    const patch = parts[2] || 0;

    if (major > 1) return 21;

    // 1.20.5+ requires Java 21
    if (minor > 20 || (minor === 20 && patch >= 5)) {
      return 21;
    }

    // 1.18 to 1.20.4 requires Java 17
    if (minor >= 18) {
      return 17;
    }

    // 1.17 requires Java 16
    if (minor === 17) {
      return 16;
    }

    // 1.16.5 and below requires Java 8
    return 8;
  }

  /**
   * Selects the most compatible installed Java version for the container.
   */
  public static findBestJavaForMinecraft(
    minecraftVersion: string,
    installations: JavaInstallation[]
  ): JavaInstallation | null {
    if (!installations || installations.length === 0) return null;

    const required = JavaDetector.getRequiredJavaVersion(minecraftVersion);

    // Exact major match
    const exact = installations.find(j => j.majorVersion === required);
    if (exact) return exact;

    // Compatible match:
    // If required is 8, 8 or 11 can often work for vanilla, but 8 is safest.
    // If required is 17 or 21, Java 21 can often run Java 17 games with compatibility flags.
    if (required >= 17) {
      const modern = installations.find(j => j.majorVersion >= required);
      if (modern) return modern;
    }

    // Fallback: return the newest installed Java
    return installations[0];
  }

  private static findWindowsJavaPaths(set: Set<string>): void {
    const programFiles = [
      process.env['ProgramFiles'],
      process.env['ProgramFiles(x86)'],
      process.env['LOCALAPPDATA'] ? path.join(process.env['LOCALAPPDATA'], 'Programs') : ''
    ].filter(Boolean) as string[];

    const subfolders = [
      'Java',
      'Eclipse Adoptium',
      'BellSoft',
      'Amazon Corretto',
      'Zulu',
      'Microsoft'
    ];

    for (const base of programFiles) {
      for (const sub of subfolders) {
        const fullDir = path.join(base, sub);
        if (fs.existsSync(fullDir)) {
          try {
            const entries = fs.readdirSync(fullDir);
            for (const entry of entries) {
              const binJava = path.join(fullDir, entry, 'bin', 'java.exe');
              if (fs.existsSync(binJava)) {
                set.add(binJava);
              }
            }
          } catch {}
        }
      }
    }
  }

  private static findLinuxJavaPaths(set: Set<string>): void {
    const searchDirs = ['/usr/lib/jvm', '/usr/java', path.join(process.env.HOME || '', '.sdkman/candidates/java')];
    for (const dir of searchDirs) {
      if (fs.existsSync(dir)) {
        try {
          const entries = fs.readdirSync(dir);
          for (const entry of entries) {
            const binJava = path.join(dir, entry, 'bin', 'java');
            if (fs.existsSync(binJava)) {
              set.add(binJava);
            }
          }
        } catch {}
      }
    }
  }

  private static findMacJavaPaths(set: Set<string>): void {
    const dir = '/Library/Java/JavaVirtualMachines';
    if (fs.existsSync(dir)) {
      try {
        const entries = fs.readdirSync(dir);
        for (const entry of entries) {
          const binJava = path.join(dir, entry, 'Contents', 'Home', 'bin', 'java');
          if (fs.existsSync(binJava)) {
            set.add(binJava);
          }
        }
      } catch {}
    }
  }
}
