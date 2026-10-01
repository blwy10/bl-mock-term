// End-to-end test: launches the real mockterm.exe with --remote-debugging-port,
// attaches Playwright over CDP, opens every screen from the command line,
// verifies live ticks change the DOM, and saves a screenshot per screen.
//
// Usage (from scripts/):  npm ci && npm run e2e [-- --attach] [-- --exe <path>]
//   --attach   connect to an already-running instance instead of launching one
import { spawn, type ChildProcess } from 'node:child_process';
import { mkdirSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { chromium, type Frame, type Page } from 'playwright-core';

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const PORT = 9222;
const CDP = `http://127.0.0.1:${PORT}`;
const args = process.argv.slice(2);
const attach = args.includes('--attach');
const exeIdx = args.indexOf('--exe');
const EXE = exeIdx >= 0 ? resolve(args[exeIdx + 1]) : join(ROOT, 'build', 'host', 'Release', 'mockterm.exe');
const OUT = join(ROOT, 'artifacts', 'screenshots');

const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

function check(cond: unknown, msg: string): asserts cond {
  if (!cond) throw new Error(`Assertion failed: ${msg}`);
  console.log(`  ok - ${msg}`);
}

async function waitForCdp(timeoutMs = 30_000): Promise<void> {
  const end = Date.now() + timeoutMs;
  while (Date.now() < end) {
    try {
      const r = await fetch(`${CDP}/json/version`);
      if (r.ok) return;
    } catch {
      /* not up yet */
    }
    await sleep(250);
  }
  throw new Error(`CDP endpoint ${CDP} did not come up`);
}

async function runCommand(page: Page, command: string, mnemonic: string): Promise<Frame> {
  const input = page.getByTestId('command-line');
  await input.click();
  await input.fill(command);
  await input.press('Enter');
  const panel = page.locator(`iframe[data-testid="panel-${mnemonic}"]`).last();
  await panel.waitFor({ state: 'visible', timeout: 10_000 });
  const frame = await (await panel.elementHandle())!.contentFrame();
  if (!frame) throw new Error(`no frame for ${mnemonic}`);
  await frame.waitForLoadState('domcontentloaded');
  return frame;
}

async function sampleUntilChanged(read: () => Promise<string>, label: string, timeoutMs = 8_000): Promise<void> {
  const first = await read();
  const end = Date.now() + timeoutMs;
  let seen = first;
  let changes = 0;
  while (Date.now() < end) {
    await sleep(500);
    const now = await read();
    if (now !== seen) {
      changes++;
      seen = now;
    }
  }
  check(changes >= 2, `${label} updated live (${changes} DOM changes in ${timeoutMs / 1000}s)`);
}

async function shot(page: Page, name: string): Promise<void> {
  await sleep(600);
  await page.screenshot({ path: join(OUT, `${name}.png`) });
  console.log(`  saved ${name}.png`);
}

async function main(): Promise<void> {
  mkdirSync(OUT, { recursive: true });
  let proc: ChildProcess | undefined;
  if (!attach) {
    console.log(`Launching ${EXE}`);
    proc = spawn(EXE, [`--remote-debugging-port=${PORT}`], { cwd: dirname(EXE), stdio: 'ignore' });
  }
  try {
    await waitForCdp();
    const browser = await chromium.connectOverCDP(CDP);
    const context = browser.contexts()[0];
    let page = context.pages().find((p) => p.url().startsWith('mockterm://'));
    for (let i = 0; !page && i < 40; i++) {
      await sleep(250);
      page = context.pages().find((p) => p.url().startsWith('mockterm://'));
    }
    if (!page) throw new Error('MockTerm page not found');
    await page.waitForLoadState('domcontentloaded');
    await page.setViewportSize({ width: 1600, height: 940 }).catch(() => undefined);

    console.log('Launchpad');
    check(await page.evaluate(() => typeof window.mockterm === 'object'), 'window.mockterm bridge is exposed');
    await page.getByTestId('launchpad').waitFor();
    await sampleUntilChanged(() => page!.getByTestId('ticker-strip').innerText(), 'ticker strip');
    await shot(page, '0-launchpad');

    console.log('AAPL US Equity DES');
    const des = await runCommand(page, 'aapl us equity des', 'DES');
    await des.waitForFunction(() => document.getElementById('last')?.textContent !== '--');
    check((await des.locator('#sec').innerText()).includes('AAPL US Equity'), 'DES shows AAPL US Equity');
    check((await des.locator('#fund').innerText()).length > 0, 'DES shows fundamentals');
    await sampleUntilChanged(() => des.locator('#quote').innerText(), 'DES quote');
    await shot(page, '1-des');

    console.log('WEI');
    const wei = await runCommand(page, 'WEI', 'WEI');
    await wei.waitForSelector('tr[data-ticker]');
    const rows = await wei.locator('tr[data-ticker]').count();
    check(rows >= 8, `WEI lists ${rows} indices`);
    await sampleUntilChanged(
      () => wei.locator('tr[data-ticker] td:nth-child(3)').allInnerTexts().then((t) => t.join('|')),
      'WEI last prices',
    );
    await shot(page, '2-wei');

    console.log('TOP');
    const top = await runCommand(page, 'TOP', 'TOP');
    await top.waitForSelector('#news tbody tr');
    check((await top.locator('#news tbody tr').count()) >= 5, 'TOP lists headlines');
    await top.locator('#news tbody tr').first().click();
    check((await top.locator('#story').innerText()).length > 40, 'TOP shows story body');
    await shot(page, '3-top');

    console.log('IB');
    const ib = await runCommand(page, 'IB', 'IB');
    await ib.getByTestId('ib-input').fill('where is AAPL trading?');
    await ib.getByTestId('ib-input').press('Enter');
    await ib.waitForSelector('#log .m.out');
    await ib.waitForSelector('#log .m.in', { timeout: 10_000 });
    check((await ib.locator('#log .m.in').last().innerText()).length > 0, 'IB counterparty replied');
    await shot(page, '4-ib');

    console.log('EURUSD Curncy GP');
    const gp = await runCommand(page, 'EURUSD Curncy GP', 'GP');
    await gp.waitForFunction(() => document.getElementById('last')?.textContent !== '--');
    check(await gp.evaluate(() => (document.getElementById('chart') as HTMLCanvasElement).width > 100), 'GP canvas rendered');
    await sampleUntilChanged(() => gp.locator('#last').innerText(), 'GP last price');
    await shot(page, '5-gp');

    console.log('Tiled layout (F2)');
    await page.getByTestId('command-line').press('F2');
    await shot(page, '6-tiled');
    await page.getByTestId('command-line').press('F2');

    console.log('Invalid command');
    await page.getByTestId('command-line').fill('FOO US Equity DES');
    await page.getByTestId('command-line').press('Enter');
    check((await page.getByTestId('command-status').innerText()).includes('Unknown security'), 'invalid command reports an error');

    await browser.close();
    console.log('\nE2E PASSED');
  } finally {
    proc?.kill();
  }
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
