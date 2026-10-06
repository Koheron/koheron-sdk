class App {
  private client: Client;
  private control: Control;
  private dpll: Dpll;
  private monitor: DpllMonitor;
  private clockGenerator: ClockGenerator;
  private stopped = false;
  private timer: number;
  private removers: Array<() => void> = [];

  constructor(private window: Window, private document: Document, ip: string) {
    // One socket preserves command/readback ordering for live numeric edits.
    this.client = new Client(ip, 1);
    window.addEventListener('HTMLImportsLoaded', () => { void this.init(); }, {once: true});
    window.addEventListener('pagehide', () => this.shutdown());
    window.addEventListener('beforeunload', () => this.shutdown());
    window.addEventListener('pageshow', event => {
      if (event.persisted) { window.location.reload(); }
    });
  }

  private async init(): Promise<void> {
    if (this.stopped) { return; }
    try {
      new Imports(this.document);
      await this.client.init();
      if (this.stopped) { return; }
      this.dpll = new Dpll(this.client);
      this.clockGenerator = new ClockGenerator(this.client);
      this.control = new Control(this.document, this.dpll, error => this.fail(error));
      for (const input of Array.from(this.document.querySelectorAll<HTMLInputElement>('.clkgen-input'))) {
        const listener = () => {
          try { this.clockGenerator.setReferenceClock(Number(input.value)); }
          catch (error) { this.fail(error); }
        };
        input.addEventListener('change', listener);
        this.removers.push(() => input.removeEventListener('change', listener));
      }
      await this.poll();
      if (this.stopped) { return; }
      this.monitor = new DpllMonitor(this.document, location.hostname, error => this.fail(error));
      await this.monitor.init();
    } catch (error) { this.fail(error); }
  }

  private async poll(): Promise<void> {
    if (this.stopped) { return; }
    try {
      const [status, routes, reference, sampleRate] = await Promise.all([
        this.dpll.getControlParameters(), this.dpll.getDacOutputs(),
        this.clockGenerator.getReferenceClock(), this.clockGenerator.getDacSamplingFrequency()
      ]);
      if (this.stopped) { return; }
      if (!Number.isFinite(sampleRate) || sampleRate <= 0) { throw new Error('Invalid DPLL sample rate.'); }
      this.control.render(status, routes, sampleRate);
      for (const input of Array.from(this.document.querySelectorAll<HTMLInputElement>('.clkgen-input'))) {
        input.checked = Number(input.value) === reference;
      }
      this.document.getElementById('board-label').textContent = `ALPHA250 · ${sampleRate / 1e6} MS/s`;
      (this.document.getElementById('instrument-controls') as HTMLFieldSetElement).disabled = false;
      const connection = this.document.getElementById('connection-status');
      connection.textContent = 'Connected';
      connection.dataset.state = 'live';
      // The next poll starts after this one completes; no overlapping frame loop.
      this.timer = this.window.setTimeout(() => { void this.poll(); }, 250);
    } catch (error) { this.fail(error); }
  }

  private fail(error: unknown): void {
    if (this.stopped) { return; }
    const status = this.document.getElementById('connection-status');
    status.textContent = 'Disconnected';
    status.dataset.state = 'error';
    this.document.getElementById('connection-error').hidden = false;
    console.error('DPLL connection failed:', error);
    this.shutdown();
  }

  private shutdown(): void {
    if (this.stopped) { return; }
    this.stopped = true;
    this.window.clearTimeout(this.timer);
    (this.document.getElementById('instrument-controls') as HTMLFieldSetElement).disabled = true;
    this.control?.dispose();
    this.monitor?.dispose();
    this.removers.forEach(remove => remove());
    this.client.exit();
  }
}

let app = new App(window, document, location.hostname);
