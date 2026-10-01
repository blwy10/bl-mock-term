import { mt, securityMap } from '../common/bridge';
import { el, setNumber } from '../common/dom';
import { fmtPct, fmtPrice, signClass, shortTicker } from '../common/format';
import type { ConnectionState, SecurityInfo, Tick } from '../types/mockterm';
import { CommandHistory, MNEMONICS, parseCommand, suggest, type Mnemonic, type Suggestion } from './command';

interface Panel {
  key: string;
  title: string;
  tab: HTMLElement;
  body: HTMLElement;
}

const APP_PATH: Record<Mnemonic, string> = { DES: 'des', GP: 'gp', WEI: 'wei', TOP: 'top', IB: 'ib' };
const HISTORY_KEY = 'mockterm.history';
const STRIP = ['SPX Index', 'INDU Index', 'CCMP Index', 'UKX Index', 'NKY Index', 'EURUSD Curncy', 'USDJPY Curncy', 'USGG10YR Index'];

const api = mt();
const input = document.getElementById('cmd') as HTMLInputElement;
const list = document.getElementById('suggest') as HTMLUListElement;
const msg = document.getElementById('msg') as HTMLElement;
const tabs = document.getElementById('tabs') as HTMLElement;
const panels = document.getElementById('panels') as HTMLElement;
const conn = document.getElementById('conn') as HTMLElement;

const panelList: Panel[] = [];
let active: Panel | undefined;
let universe: SecurityInfo[] = [];
let suggestions: Suggestion[] = [];
let selected = -1;

function loadHistory(): string[] {
  try {
    const v = JSON.parse(localStorage.getItem(HISTORY_KEY) ?? '[]');
    return Array.isArray(v) ? v.filter((x): x is string => typeof x === 'string') : [];
  } catch {
    return [];
  }
}
const history = new CommandHistory(loadHistory());

function status(text: string, error = false): void {
  msg.textContent = text;
  msg.classList.toggle('error', error);
}

const CONNECTION_LABEL: Record<ConnectionState, string> = {
  connecting: 'CONNECTING',
  live: 'LIVE',
  stale: 'STALE',
  down: 'DISCONNECTED',
};

// Dispatched by the native host whenever the mtcomm/mtserver path changes state.
window.addEventListener('mockterm:status', (e) => {
  const state = (e as CustomEvent<ConnectionState>).detail;
  if (!(state in CONNECTION_LABEL)) return;
  conn.className = state;
  conn.textContent = CONNECTION_LABEL[state];
});

function activate(p: Panel): void {
  active = p;
  for (const x of panelList) {
    x.tab.classList.toggle('active', x === p);
    x.body.classList.toggle('active', x === p);
  }
}

function closePanel(p: Panel): void {
  if (p.key === 'HOME') return;
  const i = panelList.indexOf(p);
  panelList.splice(i, 1);
  p.tab.remove();
  p.body.remove();
  if (active === p) activate(panelList[Math.max(0, i - 1)]);
}

function addPanel(key: string, title: string, body: HTMLElement): Panel {
  const existing = panelList.find((p) => p.key === key);
  if (existing) {
    activate(existing);
    return existing;
  }
  const tab = el('div', { class: 'tab', 'data-testid': `tab-${key}` });
  tab.append(el('span', {}, title));
  if (key !== 'HOME') {
    const x = el('span', { class: 'x', title: 'Close' }, '\u00d7');
    tab.append(x);
    x.addEventListener('click', (e) => {
      e.stopPropagation();
      closePanel(panel);
    });
  }
  body.classList.add('panel');
  const panel: Panel = { key, title, tab, body };
  tab.addEventListener('click', () => activate(panel));
  tabs.append(tab);
  panels.append(body);
  panelList.push(panel);
  activate(panel);
  return panel;
}

function run(command: string): boolean {
  const parsed = parseCommand(command, universe);
  if (!parsed.ok) {
    status(parsed.error, true);
    return false;
  }
  const { mnemonic, security, normalized } = parsed;
  const qs = security ? `?sec=${encodeURIComponent(security)}` : '';
  const frame = el('iframe', {
    src: `mockterm://apps/${APP_PATH[mnemonic]}/index.html${qs}`,
    title: normalized,
    'data-testid': `panel-${mnemonic}`,
  });
  const body = el('div', { 'data-app': mnemonic });
  body.append(frame);
  const title = security ? `${shortTicker(security)} ${mnemonic}` : mnemonic;
  addPanel(normalized, title, body);
  history.push(normalized);
  localStorage.setItem(HISTORY_KEY, JSON.stringify(history.all()));
  status(`${normalized} \u2014 ${MNEMONICS[mnemonic].title}`);
  return true;
}

function renderSuggestions(): void {
  list.replaceChildren();
  list.hidden = suggestions.length === 0;
  suggestions.forEach((s, i) => {
    const li = el('li', { class: i === selected ? 'sel' : '' });
    li.append(el('span', {}, s.label), el('span', { class: 'detail' }, s.detail));
    li.addEventListener('mousedown', (e) => {
      e.preventDefault();
      accept(s);
    });
    list.append(li);
  });
}

function refreshSuggestions(): void {
  suggestions = document.activeElement === input ? suggest(input.value, universe) : [];
  selected = -1;
  renderSuggestions();
}

function accept(s: Suggestion): void {
  input.value = s.value;
  refreshSuggestions();
  input.focus();
}

function submit(): void {
  const value = input.value.trim();
  if (!value) return;
  if (run(value)) input.value = '';
  suggestions = [];
  renderSuggestions();
}

input.addEventListener('input', refreshSuggestions);
input.addEventListener('focus', refreshSuggestions);
input.addEventListener('blur', () => {
  suggestions = [];
  renderSuggestions();
});
input.addEventListener('keydown', (e) => {
  if (e.key === 'Enter') {
    e.preventDefault();
    if (selected >= 0 && suggestions[selected] && suggestions[selected].value.endsWith(' ')) {
      accept(suggestions[selected]);
    } else {
      if (selected >= 0 && suggestions[selected]) input.value = suggestions[selected].value;
      submit();
    }
  } else if (e.key === 'Tab') {
    if (suggestions.length > 0) {
      e.preventDefault();
      accept(suggestions[Math.max(selected, 0)]);
    }
  } else if (e.key === 'ArrowDown') {
    e.preventDefault();
    if (suggestions.length > 0 && (selected >= 0 || input.value.trim() !== '')) {
      selected = (selected + 1) % suggestions.length;
      renderSuggestions();
      return;
    }
    const v = history.next();
    if (v !== undefined) input.value = v;
  } else if (e.key === 'ArrowUp') {
    e.preventDefault();
    if (selected >= 0) {
      selected--;
      renderSuggestions();
      return;
    }
    const v = history.prev(input.value);
    if (v !== undefined) {
      input.value = v;
      suggestions = [];
      renderSuggestions();
    }
  } else if (e.key === 'Escape') {
    input.value = '';
    history.reset();
    suggestions = [];
    renderSuggestions();
  }
});

document.addEventListener('keydown', (e) => {
  if (e.key === 'F2') {
    panels.classList.toggle('tiled');
    e.preventDefault();
  } else if (e.ctrlKey && e.key.toLowerCase() === 'w' && active) {
    closePanel(active);
    e.preventDefault();
  } else if (document.activeElement !== input && e.key.length === 1 && !e.ctrlKey && !e.altKey) {
    input.focus();
  }
});

// Apps call mockterm.openApp(); the host forwards it here.
window.addEventListener('mockterm:open', (e) => {
  const detail = (e as CustomEvent<unknown>).detail;
  if (typeof detail === 'string') run(detail);
});

function buildHome(): HTMLElement {
  const home = el('div', { class: 'home', 'data-testid': 'launchpad' });
  home.append(el('h1', {}, 'MOCKTERM LAUNCHPAD'));
  home.append(el('div', { class: 'muted' }, 'Type a command and press <GO> (Enter). Tab autocompletes, \u2191/\u2193 recalls history, F2 tiles panels, Ctrl+W closes.'));
  const table = el('table', { class: 'grid' });
  const head = el('tr');
  for (const h of ['Code', 'Function', 'Example']) head.append(el('th', {}, h));
  table.append(head);
  const examples: Record<Mnemonic, string> = {
    DES: 'AAPL US Equity DES',
    GP: 'EURUSD Curncy GP',
    WEI: 'WEI',
    TOP: 'TOP',
    IB: 'IB',
  };
  for (const m of Object.values(MNEMONICS)) {
    const tr = el('tr');
    const ex = el('td', { class: 'ex' }, examples[m.code]);
    ex.addEventListener('click', () => run(examples[m.code]));
    tr.append(el('td', { class: 'code' }, m.code), el('td', {}, m.title), ex);
    table.append(tr);
  }
  home.append(table);
  return home;
}

function buildStrip(): void {
  const strip = document.getElementById('ticker-strip') as HTMLElement;
  const cells = new Map<string, { px: HTMLElement; chg: HTMLElement }>();
  for (const t of STRIP) {
    const item = el('span', { 'data-ticker': t });
    const px = el('span', { class: 'px' }, '--');
    const chg = el('span', { class: 'chg' }, '');
    item.append(el('span', { class: 'sym' }, shortTicker(t)), px, document.createTextNode(' '), chg);
    strip.append(item);
    cells.set(t, { px, chg });
  }
  api.subscribe(STRIP, (t: Tick) => {
    const c = cells.get(t.ticker);
    if (!c) return;
    const dec = universe.find((s) => s.ticker === t.ticker)?.decimals ?? 2;
    setNumber(c.px, fmtPrice(t.last, dec), t.last);
    c.chg.textContent = fmtPct(t.changePct);
    c.chg.className = `chg ${signClass(t.change)}`;
  });
}

function tickClock(): void {
  const clock = document.getElementById('clock') as HTMLElement;
  const now = new Date();
  clock.textContent = `${now.toISOString().slice(0, 10)} ${now.toTimeString().slice(0, 8)}`;
}

addPanel('HOME', 'LAUNCHPAD', buildHome());
buildStrip();
tickClock();
setInterval(tickClock, 1000);
securityMap().then((m) => {
  universe = [...m.values()];
  status(`${universe.length} securities loaded. Try: AAPL US Equity DES`);
});
input.focus();
