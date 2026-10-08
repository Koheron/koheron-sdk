class PhaseNoiseAnalyzerApp {
  private disposed = false;
  private events = new InstrumentEvents();
  private saveConfig: PnaSaveConfig;
  private measurements: PnaMeasurementReadout;
  private updatingControls = false;
  private numbers: {[field: string]: DigitInput} = {};
  public nPoints: number;

  constructor(private document: Document, private driver: PhaseNoiseAnalyzer,
      private onConnectionError: (error: unknown) => void = () => {}) {}

  dispose(): void {
    this.disposed = true;
    this.events.dispose();
    this.measurements?.clear();
    this.saveConfig?.dispose();
    Object.keys(this.numbers).forEach(key => this.numbers[key].dispose());
  }

  setSampleRate(rate: number): void {
    for (const channel of [0, 1, 2, 3])
      this.numbers['lo' + channel]?.setLimits(rate / 2, 1E-3);
  }

  async init(): Promise<void> {
    const p = await this.driver.getParameters();
    const nominal = await this.driver.getNominalFrequencies();
    if (this.disposed) { return; }
    this.nPoints = p.data_size;
    this.measurements = new PnaMeasurementReadout(this.document);
    const number = (selector: string, value: number, commit: (value: number) => void,
        read: (p: IParameters) => number) => {
      const input = this.document.querySelector<HTMLInputElement>(selector);
      return new NumberInput(input, {value, minimum: Number(input.min), maximum: Number(input.max),
        resolution: 1, integer: true, step: selector === '.cic-rate-input' ? 2 : 1,
        validate: value => {
          if (selector === '.cic-rate-input' && value % 2 !== 0) throw new Error('Use an even decimation rate.');
        },
        commit: async value => {
          commit(value); return read(await this.driver.getParameters());
        }});
    };
    this.numbers.cic = number('.cic-rate-input', p.cic_rate,
      value => this.driver.setCicRate(value), p => p.cic_rate);
    this.numbers.navg = number('.plot-navg-input', p.fft_navg,
      value => this.driver.setFFTNavg(value), p => p.fft_navg);
    nominal.forEach((value, channel) => {
      const input = this.document.querySelector<HTMLInputElement>('.dds-input' + channel);
      this.numbers['lo' + channel] = new FrequencyInput(input, input.parentElement.querySelector('.lo-unit'), {
        value, maximum: p.fs * p.cic_rate, inclusiveMaximum: true, resolution: 1E-3, // Tune in millihertz; the DDS accepts finer steps.
        commit: async frequency => {
          this.driver.setLocalOscillator(channel, frequency);
          await this.driver.getParameters();
          return (await this.driver.getNominalFrequencies())[channel];
        }
      });
    });
    this.document.querySelectorAll<HTMLInputElement>('.channel-input').forEach(input => {
      this.events.listen(input, 'change', () => this.driver.setChannel(Number(input.value)));
    });
    const tracking = this.document.querySelector<HTMLInputElement>('.tracking-enabled-input');
    this.events.listen(tracking, 'change', () => this.driver.setTrackingEnabled(tracking.checked));
    this.events.listen(this.document.querySelector('.reset-cumulative-averager-btn'), 'click', () =>
      this.driver.resetCumulativeAverager());
    this.saveConfig = new PnaSaveConfig(this.document, () => this.driver.saveConfig(), this.onConnectionError);
    void this.updateControls();
    void this.updateMeasurements();
  }

  private async updateControls(): Promise<void> {
    if (this.disposed || this.updatingControls) { return; }
    this.updatingControls = true;
    try {
      const p = await this.driver.getParameters();
      const nominal = await this.driver.getNominalFrequencies();
      const tracking = await this.driver.getTrackingParameters();
      const average = await this.driver.getAverageStatus();
      if (this.disposed) { return; }
      this.document.querySelectorAll<HTMLInputElement>('.channel-input').forEach(input => {
        input.checked = Number(input.value) === p.channel;
      });
      this.numbers.cic.setValue(p.cic_rate);
      this.numbers.navg.setValue(p.fft_navg);
      nominal.forEach((frequency, channel) => this.numbers['lo' + channel].setValue(frequency));
      const cumulative = p.channel === 2;
      const navg = this.document.querySelector<HTMLInputElement>('.plot-navg-input');
      navg.disabled = cumulative;
      navg.hidden = cumulative;
      (this.document.querySelector('.reset-cumulative-averager-btn') as HTMLButtonElement).hidden = !cumulative;
      const status = this.document.getElementById('average-status');
      status.textContent = cumulative ? `${average.count}` : `${average.count}/`;
      status.dataset.state = average.count === 0 ? 'waiting' : cumulative ? 'cumulative'
        : average.count < average.target ? 'filling' : 'full';
      status.title = cumulative ? `${average.count} fresh synchronized X/Y windows; averaging continues until reset.`
        : `${average.count} of ${average.target} spectra in the rolling average.`;
      status.setAttribute('aria-label', status.title);
      this.document.querySelector<HTMLInputElement>('.tracking-enabled-input').checked = tracking.tracking_enabled;
      const fixed = (value: number) => Number.isFinite(value) ? value.toFixed(3) : '—';
      this.document.querySelector('.tracking-effective-bandwidth').textContent = fixed(tracking.effective_tracking_bandwidth);
      this.document.querySelector('.tracking-correction-x').textContent = fixed(tracking.tracking_correction_x);
      this.document.querySelector('.tracking-correction-y').textContent = fixed(tracking.tracking_correction_y);
      this.document.querySelector('.tracking-state').textContent = !tracking.tracking_enabled ? 'Off'
        : tracking.tracking_locked ? 'Locked' : 'Acquiring';
      const reference = this.document.querySelector<HTMLInputElement>(
        `[data-command='setReferenceClock'][value='${p.clkIndex}']`);
      if (reference) { reference.checked = true; }
    } catch (error) {
      if (!this.disposed) { this.onConnectionError(error); }
    } finally {
      if (!this.disposed) { setTimeout(() => {
        this.updatingControls = false;
        void this.updateControls();
      }, 500); }
    }
  }

  private async updateMeasurements(): Promise<void> {
    if (this.disposed) { return; }
    try {
      const m = await this.driver.getMeasurements(400);
      if (this.disposed) { return; }
      this.measurements.render(m);
    } catch (error) {
      if (!this.disposed) { this.onConnectionError(error); }
    } finally {
      if (!this.disposed) { setTimeout(() => this.updateMeasurements(), 250); }
    }
  }
}
