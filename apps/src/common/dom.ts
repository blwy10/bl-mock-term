export function el<K extends keyof HTMLElementTagNameMap>(
  tag: K,
  attrs: Record<string, string> = {},
  text?: string,
): HTMLElementTagNameMap[K] {
  const e = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs)) {
    if (k === 'class') e.className = v;
    else e.setAttribute(k, v);
  }
  if (text !== undefined) e.textContent = text;
  return e;
}

export function $(selector: string, root: ParentNode = document): HTMLElement {
  const e = root.querySelector<HTMLElement>(selector);
  if (!e) throw new Error(`missing element ${selector}`);
  return e;
}

/** Briefly highlights |e| green/red depending on the direction of a change. */
export function flash(e: HTMLElement, direction: number): void {
  if (direction === 0) return;
  e.classList.remove('flash-up', 'flash-down');
  void e.offsetWidth; // restart the CSS animation
  e.classList.add(direction > 0 ? 'flash-up' : 'flash-down');
}

/** Sets text and flashes when the numeric value changes. */
export function setNumber(e: HTMLElement, text: string, value: number): void {
  const prev = Number(e.dataset.value);
  e.textContent = text;
  e.dataset.value = String(value);
  if (Number.isFinite(prev) && prev !== value) flash(e, value - prev);
}
