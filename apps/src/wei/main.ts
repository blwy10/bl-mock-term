import { mt, securityMap } from '../common/bridge';
import { el, setNumber } from '../common/dom';
import { fmtChange, fmtPct, fmtPrice, fmtTime, shortTicker, signClass } from '../common/format';
import type { SecurityInfo, Tick } from '../types/mockterm';

const REGIONS = ['Americas', 'EMEA', 'APAC'];

interface Row {
  info: SecurityInfo;
  cells: Record<'last' | 'chg' | 'pct' | 'high' | 'low' | 'prev' | 'time', HTMLElement>;
}

async function main(): Promise<void> {
  const api = mt();
  const all = [...(await securityMap()).values()];
  const indices = all.filter((s) => s.assetClass === 'Index');
  const tbody = document.querySelector('#wei tbody') as HTMLElement;
  const rows = new Map<string, Row>();

  for (const region of REGIONS) {
    const members = indices.filter((s) => s.region === region);
    if (members.length === 0) continue;
    const group = el('tr', { class: 'group' });
    group.append(el('td', { colspan: '9' }, region));
    tbody.append(group);
    for (const info of members) {
      const tr = el('tr', { 'data-ticker': info.ticker });
      const mk = (cls = 'num') => el('td', { class: cls }, '--');
      const cells = { last: mk(), chg: mk(), pct: mk(), high: mk(), low: mk(), prev: mk(), time: mk('num muted') };
      tr.append(
        el('td', { class: 'amber' }, shortTicker(info.ticker)),
        el('td', {}, info.name),
        cells.last, cells.chg, cells.pct, cells.high, cells.low, cells.prev, cells.time,
      );
      tr.addEventListener('dblclick', () => api.openApp(`${info.ticker} GP`));
      tr.title = 'Double-click for GP chart';
      tbody.append(tr);
      rows.set(info.ticker, { info, cells });
    }
  }

  const updated = document.getElementById('updated') as HTMLElement;
  api.subscribe([...rows.keys()], (t: Tick) => {
    const row = rows.get(t.ticker);
    if (!row) return;
    const d = row.info.decimals;
    const { cells } = row;
    setNumber(cells.last, fmtPrice(t.last, d), t.last);
    cells.chg.textContent = fmtChange(t.change, d);
    cells.pct.textContent = fmtPct(t.changePct);
    const cls = signClass(t.change);
    cells.chg.className = `num ${cls}`;
    cells.pct.className = `num ${cls}`;
    cells.high.textContent = fmtPrice(t.high, d);
    cells.low.textContent = fmtPrice(t.low, d);
    cells.prev.textContent = fmtPrice(t.prevClose, d);
    cells.time.textContent = fmtTime(t.ts);
    updated.textContent = `Updated ${fmtTime(t.ts)}`;
  });
}

void main();
