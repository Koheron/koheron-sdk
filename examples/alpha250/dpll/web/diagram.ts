// The schematic follows applied telemetry only. Its channel selector never writes hardware.
class DpllDiagram {
  private svg: SVGSVGElement;
  private latest: {status: IDpllStatus, routes: number[], sampleRate: number};
  private disposed = false;
  private originals = new Map<Element, string>();
  private channel: HTMLSelectElement;
  private change = () => this.paint();

  constructor(private document: Document) {
    this.channel = document.querySelector<HTMLSelectElement>('#diagram-channel');
    this.channel.addEventListener('change', this.change);
    void this.load();
  }

  private async load(): Promise<void> {
    try {
      const response = await fetch('p_path.svg');
      if (!response.ok) { return; }
      const source = new DOMParser().parseFromString(await response.text(), 'image/svg+xml');
      if (this.disposed || source.querySelector('parsererror')) { return; }
      this.svg = this.document.importNode(source.documentElement, true) as unknown as SVGSVGElement;
      for (const text of Array.from(this.svg.querySelectorAll('[data-label], [data-gain] text, [data-gain] title, [data-integrator] title'))) {
        this.originals.set(text, text.textContent);
      }
      const host = this.document.querySelector('#live-diagram');
      host.replaceChild(this.svg, host.firstElementChild);
      this.paint();
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
      this.text(text, gain === 0 ? `${name}=0` : gain < 0 ? `−${name}` : name);
      this.text(block.querySelector('title'), `${name} = ${gain} (applied)`);
    }
    for (const block of Array.from(this.svg.querySelectorAll('[data-integrator]'))) {
      const index = Number(block.getAttribute('data-integrator'));
      const enabled = !!(status.integrators[channel] & (1 << index));
      block.setAttribute('data-state', enabled ? 'enabled' : 'disabled');
      this.text(block.querySelector('title'), `Integrator ${index + 1}: ${enabled ? 'enabled' : 'disabled · state reset to zero'}`);
    }
  }

  dispose(): void {
    this.disposed = true;
    this.channel.removeEventListener('change', this.change);
    if (!this.svg) { return; }
    this.svg.removeAttribute('data-live');
    this.originals.forEach((value, element) => this.text(element, value));
    this.text(this.svg.querySelector('[data-label="header"]'), 'READBACK UNAVAILABLE · SCHEMATIC');
  }
}
