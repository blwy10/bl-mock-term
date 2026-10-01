import { mt, param, securityMap } from '../common/bridge';
import { setNumber } from '../common/dom';
import { fmtChange, fmtPct, fmtPrice } from '../common/format';
import type { Bar, HistoryRange, Tick } from '../types/mockterm';

const AMBER = '#ffa028';
const GRID = '#1e1e1e';
const TEXT = '#8a8a8a';

const canvas = document.getElementById('chart') as HTMLCanvasElement;
let bars: Bar[] = [];
let range: HistoryRange = '1D';
let decimals = 2;
let lastTick: Tick | undefined;

function fmtAxisTime(ts: number): string {
  const d = new Date(ts);
  if (range === '1D') return d.toTimeString().slice(0, 5);
  return `${d.getMonth() + 1}/${d.getDate()}`;
}

function draw(): void {
  const dpr = window.devicePixelRatio || 1;
  const w = canvas.clientWidth;
  const h = canvas.clientHeight;
  canvas.width = Math.round(w * dpr);
  canvas.height = Math.round(h * dpr);
  const ctx = canvas.getContext('2d');
  if (!ctx || bars.length === 0) return;
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.clearRect(0, 0, w, h);
  ctx.font = '11px Consolas, monospace';

  const padL = 10, padR = 80, padT = 14, padB = 70;
  const volH = 50;
  const plotH = h - padT - padB;
  let lo = Infinity, hi = -Infinity, vmax = 0;
  for (const b of bars) {
    lo = Math.min(lo, b.low);
    hi = Math.max(hi, b.high);
    vmax = Math.max(vmax, b.volume);
  }
  const span = hi - lo || 1;
  lo -= span * 0.05;
  hi += span * 0.05;
  const x = (i: number) => padL + (i / Math.max(1, bars.length - 1)) * (w - padL - padR);
  const y = (v: number) => padT + (1 - (v - lo) / (hi - lo)) * plotH;

  // grid + price axis
  ctx.strokeStyle = GRID;
  ctx.fillStyle = TEXT;
  ctx.lineWidth = 1;
  for (let i = 0; i <= 6; i++) {
    const v = lo + ((hi - lo) * i) / 6;
    const yy = Math.round(y(v)) + 0.5;
    ctx.beginPath();
    ctx.moveTo(padL, yy);
    ctx.lineTo(w - padR, yy);
    ctx.stroke();
    ctx.fillText(fmtPrice(v, decimals), w - padR + 6, yy + 4);
  }
  const ticks = 8;
  for (let i = 0; i <= ticks; i++) {
    const idx = Math.round(((bars.length - 1) * i) / ticks);
    const xx = Math.round(x(idx)) + 0.5;
    ctx.beginPath();
    ctx.moveTo(xx, padT);
    ctx.lineTo(xx, h - padB + volH * 0 + 4);
    ctx.stroke();
    ctx.fillText(fmtAxisTime(bars[idx].ts), Math.max(2, xx - 16), h - 6);
  }

  // volume
  if (vmax > 0) {
    ctx.fillStyle = 'rgba(255,160,40,0.25)';
    const bw = Math.max(1, (w - padL - padR) / bars.length - 1);
    bars.forEach((b, i) => {
      const vh = (b.volume / vmax) * volH;
      ctx.fillRect(x(i) - bw / 2, h - 20 - vh, bw, vh);
    });
  }

  // price line + fill
  const grad = ctx.createLinearGradient(0, padT, 0, padT + plotH);
  grad.addColorStop(0, 'rgba(255,160,40,0.28)');
  grad.addColorStop(1, 'rgba(255,160,40,0)');
  ctx.beginPath();
  bars.forEach((b, i) => (i === 0 ? ctx.moveTo(x(i), y(b.close)) : ctx.lineTo(x(i), y(b.close))));
  ctx.lineTo(x(bars.length - 1), padT + plotH);
  ctx.lineTo(x(0), padT + plotH);
  ctx.closePath();
  ctx.fillStyle = grad;
  ctx.fill();
  ctx.beginPath();
  bars.forEach((b, i) => (i === 0 ? ctx.moveTo(x(i), y(b.close)) : ctx.lineTo(x(i), y(b.close))));
  ctx.strokeStyle = AMBER;
  ctx.lineWidth = 1.5;
  ctx.stroke();

  // last price marker
  const last = bars[bars.length - 1].close;
  const ly = y(last);
  ctx.fillStyle = AMBER;
  ctx.fillRect(w - padR + 2, ly - 8, padR - 4, 16);
  ctx.fillStyle = '#000';
  ctx.fillText(fmtPrice(last, decimals), w - padR + 6, ly + 4);
}

async function load(r: HistoryRange, ticker: string): Promise<void> {
  range = r;
  document.querySelectorAll<HTMLButtonElement>('[data-range]').forEach((b) => b.classList.toggle('active', b.dataset.range === r));
  bars = await mt().getHistory(ticker, r);
  if (lastTick && bars.length > 0) bars[bars.length - 1].close = lastTick.last;
  updateStats();
  draw();
}

function updateStats(): void {
  const first = bars[0]?.close ?? 0;
  const lastClose = bars[bars.length - 1]?.close ?? 0;
  const stats = document.getElementById('stats')!;
  stats.textContent = `${bars.length} bars \u00b7 range ${fmtChange(lastClose - first, decimals)} (${fmtPct(first ? ((lastClose - first) / first) * 100 : 0)})`;
}

async function main(): Promise<void> {
  const api = mt();
  const ticker = param('sec') ?? 'EURUSD Curncy';
  const info = (await securityMap()).get(ticker);
  decimals = info?.decimals ?? 2;
  document.getElementById('sec')!.textContent = `${ticker} GP`;
  document.getElementById('name')!.textContent = info?.name ?? '';
  document.title = `${ticker} GP`;

  document.querySelectorAll<HTMLButtonElement>('[data-range]').forEach((b) =>
    b.addEventListener('click', () => void load(b.dataset.range as HistoryRange, ticker)),
  );
  window.addEventListener('resize', draw);

  const lastEl = document.getElementById('last')!;
  const chgEl = document.getElementById('chg')!;
  api.subscribe([ticker], (t) => {
    lastTick = t;
    setNumber(lastEl, fmtPrice(t.last, decimals), t.last);
    chgEl.textContent = `${fmtChange(t.change, decimals)} (${fmtPct(t.changePct)})`;
    chgEl.className = t.change >= 0 ? 'up' : 'down';
    if (bars.length > 0) {
      const b = bars[bars.length - 1];
      b.close = t.last;
      b.high = Math.max(b.high, t.last);
      b.low = Math.min(b.low, t.last);
      updateStats();
      draw();
    }
  });
  await load('1D', ticker);
}

void main();
