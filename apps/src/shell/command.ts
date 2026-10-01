// Pure command-line logic for the Launchpad: parsing, autocomplete, history.

export type Mnemonic = 'DES' | 'GP' | 'WEI' | 'TOP' | 'IB';
type SecurityRule = 'required' | 'optional' | 'none';

export interface MnemonicInfo {
  code: Mnemonic;
  title: string;
  security: SecurityRule;
}

export const MNEMONICS: Record<Mnemonic, MnemonicInfo> = {
  DES: { code: 'DES', title: 'Security Description / Quote', security: 'required' },
  GP: { code: 'GP', title: 'Price Chart', security: 'required' },
  WEI: { code: 'WEI', title: 'World Equity Indices', security: 'none' },
  TOP: { code: 'TOP', title: 'Top News', security: 'optional' },
  IB: { code: 'IB', title: 'Instant Messaging', security: 'none' },
};

/** Market-sector ("yellow") keys, canonical spelling. */
export const YELLOW_KEYS = ['Equity', 'Curncy', 'Index', 'Govt', 'Comdty', 'Corp'] as const;

export interface SecurityRef {
  ticker: string;
  name?: string;
}

export type ParsedCommand =
  | { ok: true; mnemonic: Mnemonic; security?: string; normalized: string }
  | { ok: false; error: string };

function isMnemonic(s: string): s is Mnemonic {
  return Object.prototype.hasOwnProperty.call(MNEMONICS, s);
}

function canonicalYellowKey(token: string): string | undefined {
  return YELLOW_KEYS.find((k) => k.toLowerCase() === token.toLowerCase());
}

/**
 * Normalises security tokens ("aapl us equity" -> "AAPL US Equity"). When a
 * universe is supplied, a bare ticker ("aapl", "eurusd") resolves to the
 * unique security whose first token matches.
 */
export function resolveSecurity(tokens: string[], universe?: readonly SecurityRef[]): string | undefined {
  if (tokens.length === 0) return undefined;
  const parts = tokens.map((t) => canonicalYellowKey(t) ?? t.toUpperCase());
  const candidate = parts.join(' ');
  if (!universe) return canonicalYellowKey(tokens[tokens.length - 1]) ? candidate : undefined;
  const exact = universe.find((s) => s.ticker.toLowerCase() === candidate.toLowerCase());
  if (exact) return exact.ticker;
  const prefix = universe.filter((s) => s.ticker.toLowerCase().startsWith(candidate.toLowerCase() + ' '));
  if (prefix.length === 1) return prefix[0].ticker;
  return undefined;
}

export function parseCommand(input: string, universe?: readonly SecurityRef[]): ParsedCommand {
  const tokens = input.trim().split(/\s+/).filter(Boolean);
  if (tokens.length === 0) return { ok: false, error: 'Enter a command, e.g. AAPL US Equity DES' };

  const last = tokens[tokens.length - 1].toUpperCase();
  let mnemonic: Mnemonic | undefined;
  let secTokens = tokens;
  if (isMnemonic(last)) {
    mnemonic = last;
    secTokens = tokens.slice(0, -1);
  }

  let security: string | undefined;
  if (secTokens.length > 0) {
    security = resolveSecurity(secTokens, universe);
    if (!security) {
      const text = secTokens.join(' ');
      return {
        ok: false,
        error: mnemonic ? `Unknown security: ${text}` : `Unknown command: ${text}`,
      };
    }
  }

  if (!mnemonic) {
    if (!security) return { ok: false, error: `Unknown command: ${input.trim()}` };
    mnemonic = 'DES';
  }

  const rule = MNEMONICS[mnemonic].security;
  if (rule === 'required' && !security) {
    return { ok: false, error: `${mnemonic} requires a security, e.g. AAPL US Equity ${mnemonic}` };
  }
  if (rule === 'none' && security) {
    return { ok: false, error: `${mnemonic} does not take a security` };
  }
  const normalized = security ? `${security} ${mnemonic}` : mnemonic;
  return security ? { ok: true, mnemonic, security, normalized } : { ok: true, mnemonic, normalized };
}

export interface Suggestion {
  value: string;
  label: string;
  detail: string;
}

/** Autocomplete for the command line: securities first, then mnemonics. */
export function suggest(input: string, universe: readonly SecurityRef[], limit = 8): Suggestion[] {
  const raw = input.replace(/^\s+/, '');
  const q = raw.toLowerCase();
  const out: Suggestion[] = [];

  // "<security> <partial mnemonic>"
  const sec = universe
    .filter((s) => q.startsWith(s.ticker.toLowerCase() + ' '))
    .sort((a, b) => b.ticker.length - a.ticker.length)[0];
  if (sec) {
    const partial = raw.slice(sec.ticker.length).trim().toUpperCase();
    for (const m of Object.values(MNEMONICS)) {
      if (m.security !== 'none' && m.code.startsWith(partial)) {
        out.push({ value: `${sec.ticker} ${m.code}`, label: `${sec.ticker} ${m.code}`, detail: m.title });
      }
    }
    return out.slice(0, limit);
  }

  if (q.length === 0) {
    return Object.values(MNEMONICS)
      .filter((m) => m.security !== 'required')
      .map((m) => ({ value: m.code, label: m.code, detail: m.title }))
      .slice(0, limit);
  }

  for (const m of Object.values(MNEMONICS)) {
    if (m.security !== 'required' && m.code.toLowerCase().startsWith(q)) {
      out.push({ value: m.code, label: m.code, detail: m.title });
    }
  }
  const byTicker = universe.filter((s) => s.ticker.toLowerCase().startsWith(q));
  const byName = universe.filter(
    (s) => !byTicker.includes(s) && q.length >= 2 && (s.name ?? '').toLowerCase().includes(q),
  );
  for (const s of [...byTicker, ...byName]) {
    out.push({ value: `${s.ticker} `, label: s.ticker, detail: s.name ?? '' });
  }
  return out.slice(0, limit);
}

/** Shell-style command history with up/down navigation. */
export class CommandHistory {
  private entries: string[];
  private cursor: number;
  private draft = '';

  constructor(initial: readonly string[] = [], private readonly max = 100) {
    this.entries = initial.slice(-max);
    this.cursor = this.entries.length;
  }

  push(command: string): void {
    const c = command.trim();
    if (c && this.entries[this.entries.length - 1] !== c) {
      this.entries.push(c);
      if (this.entries.length > this.max) this.entries.shift();
    }
    this.cursor = this.entries.length;
    this.draft = '';
  }

  /** Older entry; |current| is remembered as the draft on first call. */
  prev(current: string): string | undefined {
    if (this.entries.length === 0) return undefined;
    if (this.cursor === this.entries.length) this.draft = current;
    if (this.cursor > 0) this.cursor--;
    return this.entries[this.cursor];
  }

  /** Newer entry, or the saved draft once past the newest. */
  next(): string | undefined {
    if (this.cursor >= this.entries.length) return undefined;
    this.cursor++;
    return this.cursor === this.entries.length ? this.draft : this.entries[this.cursor];
  }

  /** Returns the cursor to the end and drops any saved draft. */
  reset(): void {
    this.cursor = this.entries.length;
    this.draft = '';
  }

  all(): readonly string[] {
    return this.entries;
  }
}
