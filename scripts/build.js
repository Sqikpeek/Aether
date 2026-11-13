import fs from 'node:fs';
import path from 'node:path';
import { execSync } from 'node:child_process';

console.log('[Build] Compiling TypeScript...');
execSync('npx tsc', { stdio: 'inherit' });

console.log('[Build] Copying UI static assets to dist/ui...');
const srcUi = path.resolve('src/ui');
const distUi = path.resolve('dist/ui');

if (!fs.existsSync(distUi)) {
  fs.mkdirSync(distUi, { recursive: true });
}

function copyDir(src, dest) {
  const entries = fs.readdirSync(src, { withFileTypes: true });
  for (const entry of entries) {
    const s = path.join(src, entry.name);
    const d = path.join(dest, entry.name);
    if (entry.isDirectory()) {
      if (!fs.existsSync(d)) fs.mkdirSync(d);
      copyDir(s, d);
    } else {
      fs.copyFileSync(s, d);
    }
  }
}

copyDir(srcUi, distUi);

console.log('[Build] Successfully built Aether Launcher in dist/');
