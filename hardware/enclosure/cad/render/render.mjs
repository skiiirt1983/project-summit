// Renders ../../out/scene.json to PNG sheets with three.js in headless Chromium.
// Usage: node render.mjs [scene.json] [out.png]     (needs: npm i, and playwright)
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const require = createRequire(import.meta.url);
const { chromium } = require('playwright');
const scenePath = path.resolve(process.argv[2] ?? path.join(here, '../../out/scene.json'));
const outPng = path.resolve(process.argv[3] ?? path.join(path.dirname(scenePath), 'peak-views.png'));
const roots = { '/out/': path.dirname(scenePath) + '/', '/three/': path.join(here, 'node_modules/three/') };
const types = { '.js': 'text/javascript', '.json': 'application/json', '.stl': 'model/stl', '.html': 'text/html' };

const server = http.createServer((req, res) => {
  const url = decodeURIComponent(req.url.split('?')[0]);
  if (url === '/') { res.setHeader('content-type', 'text/html'); return res.end(fs.readFileSync(path.join(here, 'page.html'))); }
  for (const [prefix, dir] of Object.entries(roots)) {
    if (!url.startsWith(prefix)) continue;
    const file = path.normalize(path.join(dir, url.slice(prefix.length)));
    if (!file.startsWith(dir) || !fs.existsSync(file)) break;
    res.setHeader('content-type', types[path.extname(file)] ?? 'application/octet-stream');
    return res.end(fs.readFileSync(file));
  }
  res.statusCode = 404; res.end();
}).listen(0, '127.0.0.1');
await new Promise(r => server.once('listening', r));

const browser = await chromium.launch({ args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] });
const page = await browser.newPage({ viewport: { width: 2000, height: 450 }, deviceScaleFactor: 2 });
page.on('console', m => { if (m.type() === 'error') console.error('page:', m.text()); });
page.on('pageerror', e => console.error('page error:', e.message));
await page.goto(`http://127.0.0.1:${server.address().port}/?scene=${encodeURIComponent(path.basename(scenePath))}`);
await page.waitForFunction('window.done === true', null, { timeout: 120000 });
await page.screenshot({ path: outPng });
await browser.close(); server.close();
console.log('wrote', outPng);
