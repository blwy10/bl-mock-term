import { mt, param } from '../common/bridge';
import { el } from '../common/dom';
import { fmtTime, shortTicker } from '../common/format';
import type { Headline } from '../types/mockterm';

const MAX_ROWS = 200;

async function main(): Promise<void> {
  const api = mt();
  const sec = param('sec');
  const input = document.getElementById('q') as HTMLInputElement;
  const tbody = document.querySelector('#news tbody') as HTMLElement;
  const story = document.getElementById('story') as HTMLElement;
  if (sec) {
    input.value = sec.split(' ')[0];
    document.getElementById('title')!.textContent = `Top News \u00b7 ${sec}`;
  }

  const matches = (h: Headline, q: string) => {
    if (!q) return true;
    const t = q.toLowerCase();
    return [h.headline, h.body, h.source, h.category, ...h.tickers].some((s) => s.toLowerCase().includes(t));
  };

  const show = (h: Headline, tr: HTMLElement) => {
    tbody.querySelectorAll('tr.sel').forEach((r) => r.classList.remove('sel'));
    tr.classList.add('sel');
    story.replaceChildren(
      el('h2', {}, h.headline),
      el('div', { class: 'meta' }, `${new Date(h.ts).toLocaleString()} \u00b7 ${h.source} \u00b7 ${h.category}`),
      el('p', {}, h.body),
    );
    const tk = el('div', { class: 'tickers' });
    for (const t of h.tickers) {
      const s = el('span', { title: 'Open DES' }, t);
      s.addEventListener('click', () => api.openApp(`${t} DES`));
      tk.append(s);
    }
    story.append(tk);
  };

  const row = (h: Headline, fresh: boolean) => {
    const tr = el('tr', { class: fresh ? 'fresh' : '', 'data-id': String(h.id) });
    tr.append(
      el('td', { class: 'muted' }, fmtTime(h.ts)),
      el('td', { class: 'amber' }, h.source),
      el('td', { class: 'cat' }, h.category),
      el('td', {}, h.headline),
      el('td', { class: 'muted' }, h.tickers.map(shortTicker).join(' ')),
    );
    tr.addEventListener('click', () => show(h, tr));
    return tr;
  };

  const load = async () => {
    const list = await api.getNews(input.value.trim() || undefined);
    tbody.replaceChildren(...list.slice(0, MAX_ROWS).map((h) => row(h, false)));
  };

  let timer: number | undefined;
  input.addEventListener('input', () => {
    clearTimeout(timer);
    timer = window.setTimeout(() => void load(), 150);
  });

  api.onNews((h) => {
    if (!matches(h, input.value.trim())) return;
    tbody.prepend(row(h, true));
    while (tbody.children.length > MAX_ROWS) tbody.lastElementChild?.remove();
  });
  await load();
}

void main();
