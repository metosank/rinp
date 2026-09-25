//@ts-check

import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { promisify } from 'node:util';
import zlib from 'zlib';

const gzip = promisify(zlib.gzip);

const scriptDirectory = dirname(fileURLToPath(import.meta.url));
const projectDirectory = resolve(scriptDirectory, '..');
const sourcePath = resolve(projectDirectory, 'dist', 'index.html');
const outputPath = resolve(projectDirectory, '..', 'build', 'generated', 'web_page.cpp');

const source = await readFile(sourcePath, 'utf8');
const gzipped = await gzip(source, { level: 9 });
const bytes = Buffer.from(gzipped);
const literals = Array.from(bytes, byte => `\\x${byte.toString(16).padStart(2, '0')}`);
const chunks = [];
for (let index = 0; index < literals.length; index += 24) {
  chunks.push(`    "${literals.slice(index, index + 24).join('')}"`);
}

const output = [
  '#include "web_page.h"',
  '',
  'const char HTML_GZIP_DATA[] =',
  ...chunks.map((chunk, index) => `${chunk}${index === chunks.length - 1 ? ';' : ''}`),
  'const size_t HTML_GZIP_SIZE = ' + bytes.length + ';',
  ''
].join('\n');

await mkdir(dirname(outputPath), { recursive: true });
await writeFile(outputPath, output, 'utf8');
console.log(`Embedded ${bytes.length} bytes from ${sourcePath} into ${outputPath}`);
