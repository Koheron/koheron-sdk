class PhaseNoiseAnalyzerApp {
  private disposed = false;
  private updatingControls = false;
  private numbers: {[field: string]: DigitInput} = {};
  public nPoints: number;
  public channel: number;

  constructor(private document: Document, private driver: PhaseNoiseAnalyzer,
      private onConnectionError: (error: unknown) => void = () => {}) {}

  dispose(): void {
    this.disposed = true;
    Object.keys(this.numbers).forEach(key => this.numbers[key].dispose());
  }

  async init(): Promise<void> {
    const p = await this.driver.getParameters();
    const nominal = await this.driver.getNominalFrequencies();
    if (this.disposed) { return; }
    this.nPoints = p.data_size;
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
      input.addEventListener('change', () => this.driver.setChannel(Number(input.value)));
    });
    const tracking = this.document.querySelector<HTMLInputElement>('.tracking-enabled-input');
    tracking.addEventListener('change', () => this.driver.setTrackingEnabled(tracking.checked));
    this.document.querySelector('.reset-cumulative-averager-btn').addEventListener('click', () =>
      this.driver.resetCumulativeAverager());
    const save = this.document.querySelector<HTMLButtonElement>('.save-cfg');
    save.addEventListener('click', () => {
      try {
        this.driver.saveConfig();
        save.textContent = 'Save requested';
        this.document.getElementById('save-config-status').textContent = 'Analyzer settings save requested.';
      } catch (error) { this.onConnectionError(error); }
      setTimeout(() => { if (!this.disposed) { save.textContent = 'Save settings'; } }, 2000);
    });
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
      this.channel = p.channel;
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
      status.textContent = cumulative ? `${average.count} cumulative` : `${average.count}/`;
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
      const value = (x: number, unit: string) => Number.isFinite(x) ? `${x.toFixed(2)} ${unit}` : '—';
      this.document.querySelector('.carrier-power-span').textContent = value(m.carrier_power, 'dBm');
      this.document.querySelector('.phase-jitter-span').textContent = value(m.phase_jitter * 1E3, 'mrad rms');
      this.document.querySelector('.time-jitter-span').textContent = value(m.time_jitter * 1E12, 'ps rms');
      const frequency = (f: number) => f >= 1E6 ? `${(f / 1E6).toFixed(0)} MHz`
        : f >= 1E3 ? `${(f / 1E3).toFixed(0)} kHz` : `${f.toFixed(0)} Hz`;
      this.document.getElementById('jitter-range').textContent = Number.isFinite(m.freq_lo) && Number.isFinite(m.freq_hi)
        ? `${frequency(m.freq_lo)} – ${frequency(m.freq_hi)}` : '—';
    } catch (error) {
      if (!this.disposed) { this.onConnectionError(error); }
    } finally {
      if (!this.disposed) { setTimeout(() => this.updateMeasurements(), 250); }
    }
  }
}
