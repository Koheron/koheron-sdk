// Passive single-stream PNA monitor: acquisition controls, readouts and plot.
class PnaMonitor {
  private driver: PhaseNoiseAnalyzer;
  private measurements: PnaMeasurementReadout;
  private precision: PhasePrecision;
  private plot: Plot;
  private editors: NumberInput[] = [];
  private events = new InstrumentEvents();
  private stopped = false;
  private timer: number;

  // This component owns its monitor connection and closes it on disposal.
  constructor(private document: Document, private client: Client,
      private driverName: string, private fail: (error: unknown) => void) {}
  async init(): Promise<void> {
    await this.client.init();
    if (this.stopped) { return; }
    this.measurements = new PnaMeasurementReadout(this.document);
    this.driver = new PhaseNoiseAnalyzer(this.client, this.driverName);
    const p = await this.driver.getParameters();
    if (this.stopped) { return; }
    const number = (id: string, value: number, minimum: number, maximum: number, command: (value: number) => void) => {
      const editor = new NumberInput(this.document.getElementById(id) as HTMLInputElement, {
        value, minimum, maximum, integer: true, resolution: 1,
        step: id === 'monitor-decimation' ? 2 : 1,
        validate: next => {
          if (id === 'monitor-decimation' && next % 2 !== 0) throw new Error('Use an even decimation rate.');
        },
        commit: async next => {
          try {
            command(next);
            const accepted = await this.driver.getParameters();
            return id === 'monitor-decimation' ? accepted.cic_rate : accepted.fft_navg;
          } catch (error) { this.fail(error); throw error; }
        }
      });
      this.editors.push(editor);
    };
    number('monitor-decimation', p.cic_rate, 4, 8192, value => this.driver.setCicRate(value));
    number('monitor-averages', p.fft_navg, 1, 100, value => this.driver.setFFTNavg(value));
    const listen = (element: Element, type: string, callback: () => void) => {
      const guarded = () => {
        if (this.stopped) { return; }
        try { callback(); } catch (error) { this.fail(error); }
      };
      this.events.listen(element, type, guarded);
    };
    for (const input of Array.from(this.document.querySelectorAll<HTMLInputElement>('[name="monitor-channel"]'))) {
      input.checked = Number(input.value) === p.channel;
      listen(input, 'change', () => { if (input.checked) this.driver.setChannel(Number(input.value)); });
    }
    listen(this.document.getElementById('reset-average'), 'click', () => {
      const driver = this.client.getDriver(this.driverName);
      this.client.send(Command(driver.id, driver.getCmds()['reset_average']));
    });
    this.precision = new PhasePrecision(this.client, this.document, this.driverName);
    await this.precision.init();
    if (this.stopped) { return; }
    const basics = new PlotBasics(this.document, $('#plot-placeholder'), p.data_size,
      100, p.fs / 2, -200, 0, this.driver, '', 'Offset frequency (Hz)');
    this.plot = new Plot(this.document, this.driver, basics, this.fail);
    new ExportFile(this.document, this.plot);
    for (const id of ['monitor-controls', 'plot-controls'])
      (this.document.getElementById(id) as HTMLFieldSetElement).disabled = false;
    await this.poll();
  }
  private async poll(): Promise<void> {
    if (this.stopped) { return; }
    try {
      const [p, average, m] = await Promise.all([
        this.driver.getParameters(), this.driver.getAverageStatus(), this.driver.getMeasurements(100)
      ]);
      if (this.stopped) { return; }
      this.editors[0].setValue(p.cic_rate);
      this.editors[1].setValue(p.fft_navg);
      for (const input of Array.from(this.document.querySelectorAll<HTMLInputElement>('[name="monitor-channel"]')))
        input.checked = Number(input.value) === p.channel;
      this.document.getElementById('average-status').textContent = `${average.count}/`;
      this.measurements.render(m);
      this.timer = window.setTimeout(() => { void this.poll(); }, 500);
    } catch (error) { if (!this.stopped) this.fail(error); }
  }
  dispose(): void {
    if (this.stopped) { return; }
    this.stopped = true;
    window.clearTimeout(this.timer);
    this.editors.forEach(editor => editor.dispose());
    this.events.dispose();
    this.precision?.dispose();
    this.plot?.markUnavailable('Disconnected');
    this.plot?.dispose();
    for (const id of ['monitor-controls', 'plot-controls'])
      (this.document.getElementById(id) as HTMLFieldSetElement).disabled = true;
    this.document.querySelectorAll('.carrier-power-span, .phase-jitter-span, .time-jitter-span, #jitter-range, #average-status')
      .forEach(node => { node.textContent = '—'; });
    this.measurements?.clear();
    this.client.exit();
  }
}
