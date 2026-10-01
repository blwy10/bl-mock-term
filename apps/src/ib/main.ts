import { mt } from '../common/bridge';
import { el } from '../common/dom';
import { fmtTime } from '../common/format';
import type { ChatMessage } from '../types/mockterm';

const CONTACTS = ['J.DOE (FX DESK)', 'A.SMITH (EQ SALES)', 'R.CHEN (RATES)'];

function main(): void {
  const api = mt();
  const contacts = document.getElementById('contacts')!;
  const log = document.getElementById('log')!;
  const peerEl = document.getElementById('peer')!;
  const input = document.getElementById('text') as HTMLInputElement;
  const convs = new Map<string, ChatMessage[]>(CONTACTS.map((c) => [c, []]));
  let peer = CONTACTS[0];

  const render = () => {
    peerEl.textContent = `Chat with ${peer}`;
    contacts.querySelectorAll('div').forEach((d) => d.classList.toggle('active', d.dataset.peer === peer));
    log.replaceChildren(
      ...(convs.get(peer) ?? []).map((m) => {
        const div = el('div', { class: `m ${m.direction}` });
        div.append(el('span', { class: 't' }, fmtTime(m.ts)), el('span', { class: 'who' }, m.from), document.createTextNode(m.text));
        return div;
      }),
    );
    log.scrollTop = log.scrollHeight;
  };

  for (const c of CONTACTS) {
    const d = el('div', { 'data-peer': c });
    d.append(el('span', { class: 'dot' }, '\u25cf'), document.createTextNode(c));
    d.addEventListener('click', () => {
      peer = c;
      render();
      input.focus();
    });
    contacts.append(d);
  }

  api.onChat((m) => {
    const other = m.direction === 'out' ? m.to : m.from;
    if (!convs.has(other)) {
      convs.set(other, []);
    }
    convs.get(other)!.push(m);
    if (other === peer) render();
  });

  document.getElementById('form')!.addEventListener('submit', (e) => {
    e.preventDefault();
    const text = input.value.trim();
    if (!text) return;
    input.value = '';
    void api.sendChat(peer, text);
  });

  render();
  input.focus();
}

main();
