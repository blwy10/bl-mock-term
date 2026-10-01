import { describe, expect, it } from 'vitest';
import { CommandHistory, parseCommand, resolveSecurity, suggest } from '../src/shell/command';

const U = [
  { ticker: 'AAPL US Equity', name: 'Apple Inc' },
  { ticker: 'AMZN US Equity', name: 'Amazon.com Inc' },
  { ticker: 'EURUSD Curncy', name: 'Euro / US Dollar' },
  { ticker: 'SPX Index', name: 'US Large Cap 500' },
];

describe('parseCommand', () => {
  it('parses security + mnemonic', () => {
    expect(parseCommand('AAPL US Equity DES', U)).toEqual({
      ok: true, mnemonic: 'DES', security: 'AAPL US Equity', normalized: 'AAPL US Equity DES',
    });
    expect(parseCommand('EURUSD Curncy GP', U)).toMatchObject({ ok: true, mnemonic: 'GP', security: 'EURUSD Curncy' });
  });

  it('is case-insensitive and normalises yellow keys', () => {
    expect(parseCommand('  aapl us equity   gp ', U)).toMatchObject({ ok: true, normalized: 'AAPL US Equity GP' });
    expect(parseCommand('eurusd curncy gp')).toMatchObject({ ok: true, security: 'EURUSD Curncy' });
  });

  it('parses security-less functions', () => {
    for (const m of ['WEI', 'TOP', 'IB', 'wei']) {
      expect(parseCommand(m, U)).toMatchObject({ ok: true, mnemonic: m.toUpperCase(), normalized: m.toUpperCase() });
    }
  });

  it('defaults a bare security to DES and resolves short tickers', () => {
    expect(parseCommand('AAPL US Equity', U)).toMatchObject({ ok: true, mnemonic: 'DES' });
    expect(parseCommand('aapl gp', U)).toMatchObject({ ok: true, normalized: 'AAPL US Equity GP' });
    expect(parseCommand('eurusd', U)).toMatchObject({ ok: true, normalized: 'EURUSD Curncy DES' });
  });

  it('allows TOP with an optional security', () => {
    expect(parseCommand('AAPL US Equity TOP', U)).toMatchObject({ ok: true, mnemonic: 'TOP', security: 'AAPL US Equity' });
  });

  it('rejects invalid commands', () => {
    expect(parseCommand('', U).ok).toBe(false);
    expect(parseCommand('DES', U)).toMatchObject({ ok: false, error: expect.stringContaining('requires a security') });
    expect(parseCommand('GP', U).ok).toBe(false);
    expect(parseCommand('AAPL US Equity WEI', U)).toMatchObject({ ok: false, error: expect.stringContaining('does not take') });
    expect(parseCommand('FOO US Equity DES', U)).toMatchObject({ ok: false, error: 'Unknown security: FOO US Equity' });
    expect(parseCommand('HELLO', U)).toMatchObject({ ok: false, error: 'Unknown command: HELLO' });
  });
});

describe('resolveSecurity', () => {
  it('requires a yellow key without a universe', () => {
    expect(resolveSecurity(['ibm', 'us', 'equity'])).toBe('IBM US Equity');
    expect(resolveSecurity(['ibm'])).toBeUndefined();
  });
  it('rejects ambiguous prefixes', () => {
    expect(resolveSecurity(['A'], U)).toBeUndefined();
    expect(resolveSecurity([], U)).toBeUndefined();
  });
});

describe('suggest', () => {
  it('lists security-less functions for empty input', () => {
    expect(suggest('', U).map((s) => s.value)).toEqual(['WEI', 'TOP', 'IB']);
  });
  it('completes tickers and names', () => {
    expect(suggest('a', U).map((s) => s.label)).toEqual(['AAPL US Equity', 'AMZN US Equity']);
    expect(suggest('euro', U)[0].value).toBe('EURUSD Curncy ');
    expect(suggest('w', U)[0].value).toBe('WEI');
  });
  it('completes mnemonics after a security', () => {
    expect(suggest('AAPL US Equity ', U).map((s) => s.value)).toEqual([
      'AAPL US Equity DES', 'AAPL US Equity GP', 'AAPL US Equity TOP',
    ]);
    expect(suggest('aapl us equity g', U).map((s) => s.value)).toEqual(['AAPL US Equity GP']);
  });
  it('honours the limit', () => {
    expect(suggest('', U, 2)).toHaveLength(2);
  });
});

describe('CommandHistory', () => {
  it('navigates older/newer and restores the draft', () => {
    const h = new CommandHistory();
    h.push('WEI');
    h.push('TOP');
    h.push('TOP');
    expect(h.all()).toEqual(['WEI', 'TOP']);
    expect(h.prev('draft')).toBe('TOP');
    expect(h.prev('')).toBe('WEI');
    expect(h.prev('')).toBe('WEI');
    expect(h.next()).toBe('TOP');
    expect(h.next()).toBe('draft');
    expect(h.next()).toBeUndefined();
  });
  it('caps entries', () => {
    const h = new CommandHistory(['a', 'b', 'c'], 2);
    expect(h.all()).toEqual(['b', 'c']);
    h.push('d');
    expect(h.all()).toEqual(['c', 'd']);
  });
  it('ignores blanks and returns undefined when empty', () => {
    const h = new CommandHistory();
    h.push('   ');
    expect(h.prev('x')).toBeUndefined();
  });
});
