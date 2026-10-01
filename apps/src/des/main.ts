import { mt, param, securityMap } from '../common/bridge';
import { el, setNumber } from '../common/dom';
import { fmtChange, fmtPct, fmtPrice, fmtTime, fmtVolume, signClass } from '../common/format';
import type { Tick } from '../types/mockterm';

function kvTable(table: HTMLElement, rows: [string, string][]): Map<string, HTMLElement> {
  const out = new Map<string, HTMLElement>();
  for (const [k, v] of rows) {
    const tr = el('tr');
    const val = el('td', {}, v);
    tr.append(el('td', {}, k), val);
    table.append(tr);
    out.set(k, val);
  }
  return out;
}

async function main(): Promise<void> {
  const api = mt();
  const ticker = param('sec') ?? 'AAPL US Equity';
  const info = (await securityMap()).get(ticker);
  document.getElementById('sec')!.textContent = `${ticker} DES`;
  document.title = `${ticker} DES`;
  if (!info) {
    document.getElementById('desc')!.textContent = `Unknown security ${ticker}`;
    return;
  }
  const d = info.decimals;
  document.getElementById('name')!.textContent = `${info.name} \u00b7 ${info.assetClass} \u00b7 ${info.region} \u00b7 ${info.currency}`;

  const f = info.fundamentals;
  document.getElementById('desc')!.textContent =
    f?.description ?? `${info.name} (${info.assetClass}). Quoted in ${info.currency}, ${info.region} session.`;
  if (f) {
    kvTable(document.getElementById('fund')!, [
      ['Sector', f.sector],
      ['Market Cap', `${fmtPrice(f.marketCapBn, 1)}B`],
      ['P/E', fmtPrice(f.pe, 1)],
      ['EPS', fmtPrice(f.eps, 2)],
      ['Div Yield', `${fmtPrice(f.dividendYield, 2)}%`],
      ['Beta', fmtPrice(f.beta, 2)],
    ]);
  }

  const q = kvTable(document.getElementById('quote')!, [
    ['Bid', '--'], ['Ask', '--'], ['Open', '--'], ['High', '--'], ['Low', '--'], ['Prev Close', '--'], ['Volume', '--'], ['Last Update', '--'],
  ]);
  const last = document.getElementById('last')!;
  const chg = document.getElementById('chg')!;
  const pct = document.getElementById('pct')!;

  api.subscribe([ticker], (t: Tick) => {
    setNumber(last, fmtPrice(t.last, d), t.last);
    chg.textContent = fmtChange(t.change, d);
    pct.textContent = `(${fmtPct(t.changePct)})`;
    chg.className = pct.className = signClass(t.change);
    setNumber(q.get('Bid')!, fmtPrice(t.bid, d), t.bid);
    setNumber(q.get('Ask')!, fmtPrice(t.ask, d), t.ask);
    q.get('Open')!.textContent = fmtPrice(t.open, d);
    q.get('High')!.textContent = fmtPrice(t.high, d);
    q.get('Low')!.textContent = fmtPrice(t.low, d);
    q.get('Prev Close')!.textContent = fmtPrice(t.prevClose, d);
    q.get('Volume')!.textContent = info.assetClass === 'Equity' ? fmtVolume(t.volume) : 'n/a';
    q.get('Last Update')!.textContent = fmtTime(t.ts);
  });

  document.getElementById('gp')!.addEventListener('click', () => api.openApp(`${ticker} GP`));
  document.getElementById('top')!.addEventListener('click', () => api.openApp(`${ticker} TOP`));

  const tbody = document.querySelector('#news tbody') as HTMLElement;
  const symbol = ticker.split(' ')[0];
  const addNews = (h: { ts: number; source: string; headline: string }, top = false) => {
    const tr = el('tr');
    tr.append(el('td', { class: 'muted' }, fmtTime(h.ts, false)), el('td', { class: 'amber' }, h.source), el('td', {}, h.headline));
    if (top) tbody.prepend(tr);
    else tbody.append(tr);
  };
  (await api.getNews(symbol)).slice(0, 15).forEach((h) => addNews(h));
  api.onNews((h) => {
    if (h.tickers.includes(ticker)) addNews(h, true);
  });
}

void main();
