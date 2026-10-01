// Bundles each app (src/<app>/main.ts) into dist/<app>/main.js and copies
// static assets. The host serves dist/ via the mockterm:// scheme.
import { build } from 'esbuild';
import { cpSync, mkdirSync, readdirSync, rmSync, existsSync } from 'node:fs';
import { join } from 'node:path';

const APPS = ['shell', 'wei', 'des', 'gp', 'top', 'ib'];
const src = 'src';
const out = 'dist';

rmSync(out, { recursive: true, force: true });
mkdirSync(out, { recursive: true });

await build({
  entryPoints: Object.fromEntries(APPS.map((a) => [`${a}/main`, join(src, a, 'main.ts')])),
  outdir: out,
  bundle: true,
  format: 'iife',
  target: 'chrome120',
  sourcemap: 'inline',
  logLevel: 'info',
});

for (const app of APPS) {
  for (const f of readdirSync(join(src, app))) {
    if (f.endsWith('.html') || f.endsWith('.css')) cpSync(join(src, app, f), join(out, app, f));
  }
}
mkdirSync(join(out, 'common'), { recursive: true });
cpSync(join(src, 'common', 'theme.css'), join(out, 'common', 'theme.css'));
if (existsSync(join(src, 'index.html'))) cpSync(join(src, 'index.html'), join(out, 'index.html'));
console.log(`Built ${APPS.length} apps into ${out}/`);
