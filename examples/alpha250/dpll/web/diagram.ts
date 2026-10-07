// The schematic follows applied telemetry only. Its channel selector never writes hardware.
class DpllDiagram {
  private svg: SVGSVGElement;
  private latest: {status: IDpllStatus, routes: number[], sampleRate: number};
  private disposed = false;
  private originals = new Map<Element, string>();
  private channel: HTMLSelectElement;
  private removers: Array<() => void> = [];
  private change = () => { this.highlight(null); this.paint(); };

  private listen(target: EventTarget, event: string, listener: EventListener): void {
    target.addEventListener(event, listener);
    this.removers.push(() => target.removeEventListener(event, listener));
  }

  private controlFor(block: Element): HTMLElement {
    const channel = this.channel.value;
    const gain = block.getAttribute('data-gain');
    return gain ? this.document.querySelector<HTMLElement>(`.gain-row[data-channel="${channel}"][data-status="${gain}"]`) :
      this.document.querySelector<HTMLElement>(`.integrator-switch[data-channel="${channel}"][data-integratorindex="${block.getAttribute('data-integrator')}"]`);
  }

  private highlight(control: Element): void {
    for (const element of Array.from(this.document.querySelectorAll('#live-diagram [data-linked], .gain-row[data-linked], .integrators label[data-linked]'))) { element.removeAttribute('data-linked'); }
    if (!control || !this.svg) { return; }
    const row = control.closest('.gain-row');
    const input = control.closest('.integrator-switch') || control.closest('.integrators label')?.querySelector('.integrator-switch');
    const source = row || input;
    if (!source || source.getAttribute('data-channel') !== this.channel.value) { return; }
    (row || input.parentElement).setAttribute('data-linked', 'true');
    const selector = row ? `[data-gain="${row.getAttribute('data-status')}"]` : `[data-integrator="${input.getAttribute('data-integratorindex')}"]`;
    for (const block of Array.from(this.svg.querySelectorAll(selector))) { block.setAttribute('data-linked', 'true'); }
  }

  private edit(block: Element): void {
    if (this.disposed || !this.latest) { return; }
    const control = this.controlFor(block);
    control.closest('details').open = true;
    // Prefer the numeric editor when enabled; zero gains focus the selected sign button.
    const editor = control.querySelector<HTMLInputElement>('.gain-input');
    const target = editor ? (editor.disabled ? control.querySelector<HTMLElement>('.gain-button[aria-pressed="true"]') : editor) : control;
    target.focus();
    this.highlight(control);
  }

  constructor(private document: Document) {
    this.channel = document.querySelector<HTMLSelectElement>('#diagram-channel');
    this.listen(this.channel, 'change', this.change);
    this.listen(document, 'focusin', event => {
      const target = event.target as Element;
      if (!target.closest('.channel-panel')) { return; }
      const source = target.closest('[data-channel]');
      if (source) {
        this.channel.value = source.getAttribute('data-channel');
        this.paint();
        this.highlight(target);
      }
    });
    this.listen(document, 'pointerover', event => {
      const target = event.target as Element;
      if (target.closest('.channel-panel')) { this.highlight(target); }
    });
    this.listen(document, 'pointerout', event => {
      const target = event.target as Element;
      const related = (event as PointerEvent).relatedTarget as Node;
      const source = target.closest('.gain-row, .integrators label');
      if (source && (!related || !source.contains(related))) { this.highlight(this.document.activeElement); }
    });
    this.listen(document, 'focusout', event => {
      const next = (event as FocusEvent).relatedTarget as Element;
      this.highlight(next);
    });
    void this.load();
  }

  private async load(): Promise<void> {
    try {
      const response = await fetch('p_path.svg');
      if (!response.ok) { return; }
      const source = new DOMParser().parseFromString(await response.text(), 'image/svg+xml');
      if (this.disposed || source.querySelector('parsererror')) { return; }
      this.svg = this.document.importNode(source.documentElement, true) as unknown as SVGSVGElement;
      this.svg.setAttribute('role', 'group');
      for (const block of Array.from(this.svg.querySelectorAll('[data-gain], [data-integrator]'))) {
        block.setAttribute('role', 'button');
        this.listen(block, 'click', () => this.edit(block));
        this.listen(block, 'keydown', event => {
          if (['Enter', ' '].indexOf((event as KeyboardEvent).key) >= 0) { event.preventDefault(); this.edit(block); }
        });
        this.listen(block, 'pointerover', () => this.highlight(this.controlFor(block)));
        this.listen(block, 'pointerout', () => this.highlight(this.document.activeElement));
        this.listen(block, 'focus', () => this.highlight(this.controlFor(block)));
      }
      for (const text of Array.from(this.svg.querySelectorAll('[data-label], [data-gain] text, [data-gain] title, [data-integrator] title'))) {
        this.originals.set(text, text.textContent);
      }
      const host = this.document.querySelector('#live-diagram');
      host.replaceChild(this.svg, host.firstElementChild);
      this.paint();
      this.highlight(this.document.activeElement);
    } catch (_) {
      // Keep the static image if the optional live SVG cannot load.
    }
  }

  render(status: IDpllStatus, routes: number[], sampleRate: number): void {
    if (this.disposed) { return; }
    this.latest = {status, routes, sampleRate};
    this.paint();
  }

  private text(element: Element, value: string): void {
    if (element.textContent !== value) { element.textContent = value; }
  }

  private paint(): void {
    if (!this.svg || !this.latest || this.disposed) { return; }
    const {status, routes, sampleRate} = this.latest;
    const channel = Number(this.channel.value);
    const fast = !!(status.p_path[channel] & 1);
    const label = (name: string, value: string) => this.text(this.svg.querySelector(`[data-label="${name}"]`), value);
    this.svg.setAttribute('data-live', 'true');
    for (const panel of Array.from(this.document.querySelectorAll('.channel-panel'))) {
      panel.setAttribute('data-diagram-selected', String(!!panel.querySelector(`[data-channel="${channel}"]`)));
    }
    label('header', `ADC${channel} · ${sampleRate / 1e6} MS/s · APPLIED SETTINGS`);
    label('adc', `ADC${channel}`);
    label('dds', `${Number((status.dds_freq[channel] / 1e6).toPrecision(6))} MHz DDS`);
    label('mode', fast ? 'Fast selected' : 'Accurate selected');
    label('range', fast ? (status.p_path[channel] & 2 ? 'Within estimate range' : 'Outside estimate range') : 'Calibrated near lock');
    const names = ['Loop 0', 'Loop 1', 'φ₀ LSB', 'φ₁ LSB', 'φ₀ MSB', 'φ₁ MSB', 'DDS 0', 'DDS 1'];
    label('routes', routes.map((route, dac) => `DAC${dac}: ${names[route] || '?'}`).join(' · '));
    // Highlight only the selector inputs: Accurate continues to feed I², I³ and the monitor in Fast mode.
    for (const wire of Array.from(this.svg.querySelectorAll('[data-mode]'))) {
      wire.setAttribute('data-selected', String(wire.getAttribute('data-mode') === (fast ? 'fast' : 'accurate')));
    }
    for (const block of Array.from(this.svg.querySelectorAll('[data-gain]'))) {
      const gain = status[block.getAttribute('data-gain')][channel];
      const text = block.querySelector('text');
      const name = this.originals.get(text);
      block.setAttribute('data-state', gain === 0 ? 'disabled' : 'enabled');
      this.text(text, gain === 0 ? name : `${gain < 0 ? '−' : '+'} ${name}`);
      this.text(block.querySelector('.gain-db'), gain === 0 ? 'Off' : `${DpllGain.db(gain)} dB`);
      const description = `ADC ${channel} ${name}: ${DpllGain.description(gain)} (applied). Edit gain.`;
      this.text(block.querySelector('title'), description);
      block.setAttribute('aria-label', description);
      block.setAttribute('tabindex', '0');
    }
    for (const block of Array.from(this.svg.querySelectorAll('[data-integrator]'))) {
      const index = Number(block.getAttribute('data-integrator'));
      const enabled = !!(status.integrators[channel] & (1 << index));
      block.setAttribute('data-state', enabled ? 'enabled' : 'disabled');
      const description = `ADC ${channel} integrator ${index + 1}: ${enabled ? 'enabled' : 'disabled · state reset to zero'}. Go to control.`;
      this.text(block.querySelector('title'), description);
      block.setAttribute('aria-label', description);
      block.setAttribute('tabindex', '0');
    }
  }

  dispose(): void {
    this.disposed = true;
    this.removers.forEach(remove => remove());
    this.highlight(null);
    for (const panel of Array.from(this.document.querySelectorAll('[data-diagram-selected]'))) { panel.removeAttribute('data-diagram-selected'); }
    if (!this.svg) { return; }
    this.svg.removeAttribute('data-live');
    for (const block of Array.from(this.svg.querySelectorAll('[role="button"]'))) {
      block.removeAttribute('role'); block.removeAttribute('tabindex'); block.removeAttribute('aria-label');
    }
    this.originals.forEach((value, element) => this.text(element, value));
    this.text(this.svg.querySelector('[data-label="header"]'), 'READBACK UNAVAILABLE · SCHEMATIC');
  }
}
