export function fmtPrice(v: number | undefined, decimals = 2): string {
  if (v === undefined || !Number.isFinite(v)) return '--';
  return v.toLocaleString('en-US', { minimumFractionDigits: decimals, maximumFractionDigits: decimals });
}

export function fmtChange(v: number | undefined, decimals = 2): string {
  if (v === undefined || !Number.isFinite(v)) return '--';
  return (v > 0 ? '+' : '') + fmtPrice(v, decimals);
}

export function fmtPct(v: number | undefined): string {
  if (v === undefined || !Number.isFinite(v)) return '--';
  return (v > 0 ? '+' : '') + v.toFixed(2) + '%';
}

export function fmtVolume(v: number | undefined): string {
  if (v === undefined || !Number.isFinite(v) || v === 0) return '--';
  if (v >= 1e9) return (v / 1e9).toFixed(2) + 'B';
  if (v >= 1e6) return (v / 1e6).toFixed(2) + 'M';
  if (v >= 1e3) return (v / 1e3).toFixed(1) + 'K';
  return String(v);
}

export function fmtTime(ts: number, seconds = true): string {
  const d = new Date(ts);
  const p = (n: number) => String(n).padStart(2, '0');
  return `${p(d.getHours())}:${p(d.getMinutes())}${seconds ? ':' + p(d.getSeconds()) : ''}`;
}

export function signClass(v: number): string {
  return v > 0 ? 'up' : v < 0 ? 'down' : 'flat';
}

export function shortTicker(ticker: string): string {
  return ticker.split(' ')[0];
}
