import fs from 'node:fs';
import path from 'node:path';

const distDir = path.resolve('dist');
const headerPath = path.resolve('src/native/embedded_dist.h');

const targetFiles = [
  'index.js',
  'config.js',
  'core/backup.js',
  'core/containers.js',
  'core/crash_analyzer.js',
  'core/http_utils.js',
  'core/java.js',
  'core/jvm_presets.js',
  'core/launcher.js',
  'core/modrinth.js',
  'core/server_pinger.js',
  'core/updater.js',
  'server/api.js',
  'ui/index.html',
  'ui/styles.css',
  'ui/app.js',
  'ui/i18n.js',
  'package.json'
];

let out = `/* Auto-generated embedded launcher assets */
#ifndef EMBEDDED_DIST_H
#define EMBEDDED_DIST_H

#include <stddef.h>

typedef struct {
    const char* relPath;
    const unsigned char* data;
    size_t size;
} EmbeddedFile;

`;

const validEntries = [];

targetFiles.forEach((rel, idx) => {
  const full = path.join(distDir, rel);
  if (!fs.existsSync(full)) {
    console.warn(`[Embed] Warning: ${full} not found`);
    return;
  }

  const buf = fs.readFileSync(full);
  const varName = `file_data_${idx}`;
  
  out += `static const unsigned char ${varName}[] = {\n  `;
  for (let i = 0; i < buf.length; i++) {
    out += `0x${buf[i].toString(16).padStart(2, '0')},`;
    if ((i + 1) % 16 === 0) out += '\n  ';
  }
  out += '\n};\n\n';

  validEntries.push({ rel, varName, size: buf.length });
});

out += `static const EmbeddedFile EMBEDDED_FILES[] = {\n`;
for (const entry of validEntries) {
  out += `    { "${entry.rel}", ${entry.varName}, ${entry.size} },\n`;
}
out += `};\n\n`;
out += `static const size_t NUM_EMBEDDED_FILES = ${validEntries.length};\n\n`;
out += `#endif /* EMBEDDED_DIST_H */\n`;

fs.writeFileSync(headerPath, out, 'utf-8');
console.log(`[Embed] Generated ${headerPath} with ${validEntries.length} embedded files.`);
