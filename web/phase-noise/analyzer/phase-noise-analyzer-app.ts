// Interface for the Phase Noise Analyzer driver
// (c) Koheron

class PhaseNoiseAnalyzerApp {
  private disposed = false;
  private events = new InstrumentEvents();
  private saveConfig: PnaSaveConfig;
  private cicRateInput: HTMLInputElement;
  private nAvgInput: HTMLInputElement;
  private channelInputs: HTMLInputElement[];
  private measurements: PnaMeasurementReadout;

  private laserModeEnableCheckbox: HTMLInputElement;
  private interferometerDelayInput: HTMLInputElement;

  private ddsInputs: HTMLInputElement[];
  private trackingEnabledInput: HTMLInputElement;
  private averageStatus: HTMLElement;

  private numbers: {[field: string]: DigitInput} = {};
  public nPoints: number;
  public channel: number;

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
    for (const channel of [0, 1])
      this.numbers['lo' + channel]?.setLimits(rate / 2, rate / Math.pow(2, 48));
  }

  async init(): Promise<void> {
    const parameters = await this.driver.getParameters();
    if (this.disposed) { return; }
    const tracking = await this.driver.getTrackingParameters();
    if (this.disposed) { return; }
    this.nPoints = parameters.data_size;

    this.channelInputs = <HTMLInputElement[]><any>this.document.getElementsByClassName("channel-input");
    this.measurements = new PnaMeasurementReadout(this.document);

    this.ddsInputs = [0, 1].map(i =>
      this.document.querySelector<HTMLInputElement>(`.dds-input${i}`)!);
    this.initNumbers(parameters, tracking);
    this.saveConfig = new PnaSaveConfig(this.document, () => this.driver.saveConfig(), this.onConnectionError);
    this.averageStatus = this.document.querySelector('#average-status');
    this.trackingEnabledInput = this.document.querySelector('.tracking-enabled-input');
    this.trackingEnabledInput.checked = tracking.enabled;
    this.events.listen(this.trackingEnabledInput, 'change', () =>
      this.driver.setTrackingEnabled(this.trackingEnabledInput.checked));
    this.initChannelInput();
    this.initLaserMode();
    this.updateMeasurements();
    this.updateControls();
  }

  private initNumbers(parameters: IParameters, tracking: ITrackingParameters): void {
    this.cicRateInput = this.document.querySelector('.cic-rate-input');
    this.nAvgInput = this.document.querySelector('.plot-navg-input');
    this.interferometerDelayInput = this.document.querySelector('.interferometer-delay');
    const cicRateStep = Number(this.cicRateInput.step) || 1;
    const number = (input: HTMLInputElement, value: number, unitLabel: string, commit: (value: number) => void, read: (parameters: IParameters) => number) =>
      new NumberInput(input, {
        value, minimum: Number(input.min), maximum: Number(input.max), resolution: 1, integer: true, unitLabel,
        step: input === this.cicRateInput ? cicRateStep : 1,
        validate: value => {
          if (input === this.cicRateInput && value % cicRateStep !== 0) throw new Error('Use an even decimation rate.');
        },
        commit: async value => { commit(value); return read(await this.driver.getParameters()); }
      });
    this.numbers.cic = number(this.cicRateInput, parameters.cic_rate, '', value => this.driver.setCicRate(value), p => p.cic_rate);
    this.numbers.navg = number(this.nAvgInput, parameters.fft_navg, '', value => this.driver.setFFTNavg(value), p => p.fft_navg);
    this.numbers.delay = number(this.interferometerDelayInput, parameters.interferometer_delay * 1e9, 'ns',
      value => this.driver.setInterferometerDelay(value * 1e-9), p => p.interferometer_delay * 1e9);
    const adcSampleRate = parameters.fs * 2 * parameters.cic_rate;
    [tracking.nominal0, tracking.nominal1].forEach((value, channel) => {
      const input = this.ddsInputs[channel];
      this.numbers['lo' + channel] = new FrequencyInput(input, input.parentElement.querySelector('.lo-unit'), {
        value, maximum: adcSampleRate / 2, inclusiveMaximum: true, resolution: adcSampleRate / Math.pow(2, 48),
        commit: async frequency => {
          this.driver.setLocalOscillator(channel, frequency);
          await this.driver.getParameters();
          const t = await this.driver.getTrackingParameters();
          return channel === 0 ? t.nominal0 : t.nominal1;
        }
      });
    });
  }

  initChannelInput(): void {
    for (let i = 0; i < this.channelInputs.length; i++) {
      this.events.listen(this.channelInputs[i], 'change', (event) => {
        this.channel = parseInt((<HTMLInputElement>event.currentTarget).value);
        this.driver[(<HTMLInputElement>event.currentTarget).dataset.command](this.channel);
      });
    }
  }

  initLaserMode(): void {
    this.laserModeEnableCheckbox = <HTMLInputElement>this.document.getElementsByClassName("laser-mode-input")[0];
    this.interferometerDelayInput = <HTMLInputElement>this.document.getElementsByClassName("interferometer-delay")[0];

    this.events.listen(this.laserModeEnableCheckbox, "change", () => {
      const enabled: 0 | 1 = this.laserModeEnableCheckbox.checked ? 1 : 0;
      this.driver.setAnalyzerMode(enabled);
      this.interferometerDelayInput.disabled = !enabled;
    });

  }

  private async updateMeasurements() {
    if (this.disposed) { return; }
    try {
      const navg: number = 400;
      const meas = await this.driver.getMeasurements(navg);
      if (this.disposed) { return; }

      this.measurements.render(meas);
    } catch (error) {
      if (!this.disposed) { this.onConnectionError(error); }
    } finally {
      if (!this.disposed) { setTimeout(() => { this.updateMeasurements(); }, 250); }
    }
  }

  private async updateControls(): Promise<void> {
    if (this.disposed) { return; }
    try {
      const parameters = await this.driver.getParameters();
      if (this.disposed) { return; }
      const tracking = await this.driver.getTrackingParameters();
      if (this.disposed) { return; }

      if (parameters.channel == 0) {
        this.channelInputs[0].checked = true;
        this.channelInputs[1].checked = false;
      } else {
        this.channelInputs[0].checked = false;
        this.channelInputs[1].checked = true;
      }

      this.numbers.cic.setValue(parameters.cic_rate);
      this.numbers.navg.setValue(parameters.fft_navg);
      this.numbers.lo0.setValue(tracking.nominal0);
      this.numbers.lo1.setValue(tracking.nominal1);
      this.trackingEnabledInput.checked = tracking.enabled;
      const fixed = (value: number) => Number.isFinite(value) ? (Math.abs(value) < .0005 ? '0.000' : value.toFixed(3)) : '—';
      this.document.querySelector('.tracking-effective-bandwidth').textContent = fixed(tracking.effectiveBandwidth);
      this.document.querySelector('.tracking-correction-0').textContent = fixed(tracking.correction0);
      this.document.querySelector('.tracking-correction-1').textContent = fixed(tracking.correction1);
      const locked = parameters.channel === 0 ? tracking.locked0 : tracking.locked1;
      const nominal = parameters.channel === 0 ? tracking.nominal0 : tracking.nominal1;
      const paused = tracking.effectiveBandwidth <= 0 || tracking.maxStep <= 0 ||
        tracking.maxCorrection <= 0 || nominal <= 0;
      this.document.querySelector('.tracking-state').textContent =
        !tracking.enabled ? 'Off' : paused ? `ADC${parameters.channel} paused` :
        locked ? `ADC${parameters.channel} locked` : `ADC${parameters.channel} acquiring`;

      const laserModeEnabled: boolean = parameters.analyzer_mode === 'laser';
      this.laserModeEnableCheckbox.checked = laserModeEnabled;
      this.interferometerDelayInput.disabled = !laserModeEnabled;

      this.numbers.delay.setValue(parameters.interferometer_delay * 1e9);

      const referenceClock = this.document.querySelector<HTMLInputElement>(
        "[data-command='setReferenceClock'][value='" + parameters.clkIndex + "']");
      if (referenceClock) { referenceClock.checked = true; }

      await this.updateAverageProgress();
    } catch (error) {
      if (!this.disposed) { this.onConnectionError(error); }
    } finally {
      if (!this.disposed) { setTimeout(() => { this.updateControls(); }, 500); }
    }
  }

  private async updateAverageProgress(): Promise<void> {
    if (!this.averageStatus) { return; }
    try {
      const {count, target} = await this.driver.getAverageStatus();
      if (this.disposed) { return; }
      this.averageStatus.dataset.state = count === 0 ? 'waiting' : count < target ? 'filling' : 'full';
      this.averageStatus.textContent = `${count}/`;
      this.averageStatus.title = `${count} of ${target} spectra in the rolling average. ` +
        (count === 0 ? 'Waiting for acquisition.' : count < target ? 'Window filling.' : 'Full window; new spectra replace the oldest.');
      this.averageStatus.setAttribute('aria-label', `${count} of ${target} spectra averaged`);
    } catch (error) {
      if (this.disposed) { return; }
      this.averageStatus.dataset.state = 'unknown';
      this.averageStatus.textContent = '—/';
      this.averageStatus.title = 'Average progress unavailable';
      this.averageStatus.setAttribute('aria-label', 'Average progress unavailable');
      this.onConnectionError(error);
    }
  }
}
