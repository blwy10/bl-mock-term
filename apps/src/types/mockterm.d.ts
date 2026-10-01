// The only API MockTerm apps may use. It is injected into every frame by the
// native host (CefRenderProcessHandler::OnContextCreated) as window.mockterm.

export type Unsubscribe = () => void;
export type HistoryRange = '1D' | '1M' | '1Y';
export type AssetClass = 'Equity' | 'Curncy' | 'Index' | 'Rate';
/**
 * Health of the mockterm -> mtcomm -> mtserver path, delivered to the shell
 * as a `mockterm:status` CustomEvent whose detail is this value.
 */
export type ConnectionState = 'connecting' | 'live' | 'stale' | 'down';

export interface Tick {
  ticker: string;
  bid: number;
  ask: number;
  last: number;
  open: number;
  high: number;
  low: number;
  prevClose: number;
  change: number;
  changePct: number;
  volume: number;
  /** Milliseconds since the Unix epoch. */
  ts: number;
  seq: number;
}

export interface Bar {
  ts: number;
  open: number;
  high: number;
  low: number;
  close: number;
  volume: number;
}

export interface Headline {
  id: number;
  ts: number;
  source: string;
  category: string;
  tickers: string[];
  headline: string;
  body: string;
}

export interface ChatMessage {
  id: number;
  ts: number;
  from: string;
  to: string;
  text: string;
  direction: 'in' | 'out';
}

export interface Fundamentals {
  sector: string;
  marketCapBn: number;
  pe: number;
  dividendYield: number;
  eps: number;
  beta: number;
  description: string;
}

export interface SecurityInfo {
  ticker: string;
  name: string;
  assetClass: AssetClass;
  region: string;
  currency: string;
  decimals: number;
  fundamentals?: Fundamentals;
}

export interface MockTerm {
  subscribe(tickers: string[], onTick: (t: Tick) => void): Unsubscribe;
  getHistory(ticker: string, range: HistoryRange): Promise<Bar[]>;
  getNews(query?: string): Promise<Headline[]>;
  onNews(cb: (h: Headline) => void): Unsubscribe;
  sendChat(to: string, text: string): Promise<void>;
  onChat(cb: (m: ChatMessage) => void): Unsubscribe;
  /** Opens an app in the shell, e.g. "AAPL US Equity GP". */
  openApp(command: string): void;
  /** MockTerm extension: the instrument universe (for autocomplete/DES). */
  getSecurities(): Promise<SecurityInfo[]>;
}

declare global {
  interface Window {
    mockterm: MockTerm;
  }
}
