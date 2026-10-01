import type { MockTerm, SecurityInfo } from '../types/mockterm';

/** Returns the host-injected bridge, or renders an error if it is missing. */
export function mt(): MockTerm {
  const api = window.mockterm;
  if (!api) {
    document.body.innerHTML =
      '<div class="fatal">window.mockterm is not available. Apps must run inside the MockTerm host.</div>';
    throw new Error('window.mockterm missing');
  }
  return api;
}

let securities: Promise<Map<string, SecurityInfo>> | undefined;

export function securityMap(): Promise<Map<string, SecurityInfo>> {
  securities ??= mt()
    .getSecurities()
    .then((list) => new Map(list.map((s) => [s.ticker, s])));
  return securities;
}

export function param(name: string): string | undefined {
  return new URLSearchParams(location.search).get(name) ?? undefined;
}
