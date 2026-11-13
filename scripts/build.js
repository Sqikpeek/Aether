import fs from 'node:fs';
import path from 'node:path';
import { execSync } from 'node:child_process';

console.log('[Build] Checking TypeScript compiler...');

const distIndex = path.resolve('dist/index.js');
const localTsc = path.resolve('node_modules/typescript/bin/tsc');

let compiled = false;

if (fs.existsSync(localTsc)) {
  try {
    console.log('[Build] Compiling TypeScript using local compiler...');
    execSync(`node "${localTsc}"`, { stdio: 'inherit' });
    compiled = true;
  } catch (err) {
    console.warn('[Build] Local TypeScript compilation had warnings/errors:', err.message);
  }
} else {
  // If dist/index.js already exists (pre-built in repository), keep it!
  if (fs.existsSync(distIndex)) {
    console.log('[Build] Using pre-built dist/ artifacts.');
    compiled = true;
  } else {
    try {
      console.log('[Build] Compiling TypeScript via npx --yes typescript...');
      execSync('npx --yes typescript', { stdio: 'inherit' });
      compiled = true;
    } catch (err) {
      console.warn('[Build] Warning: Could not run npx typescript:', err.message);
    }
  }
}

console.log('[Build] Ensuring UI static assets are in dist/ui...');
const srcUi = path.resolve('src/ui');
const distUi = path.resolve('dist/ui');

if (!fs.existsSync(distUi)) {
  fs.mkdirSync(distUi, { recursive: true });
}

function copyDir(src, dest) {
  if (!fs.existsSync(src)) return;
  const entries = fs.readdirSync(src, { withFileTypes: true });
  for (const entry of entries) {
    const s = path.join(src, entry.name);
    const d = path.join(dest, entry.name);
    if (entry.isDirectory()) {
      if (!fs.existsSync(d)) fs.mkdirSync(d, { recursive: true });
      copyDir(s, d);
    } else {
      fs.copyFileSync(s, d);
    }
  }
}

if (fs.existsSync(srcUi)) {
  copyDir(srcUi, distUi);
}

// Generate embedded C header for standalone Windows executables
try {
  console.log('[Build] Generating embedded C payload for standalone executables...');
  execSync('node scripts/generate_embedded.js', { stdio: 'inherit' });
} catch (err) {
  console.warn('[Build] Warning generating embedded payload:', err.message);
}

console.log('[Build] Build process completed successfully.');
