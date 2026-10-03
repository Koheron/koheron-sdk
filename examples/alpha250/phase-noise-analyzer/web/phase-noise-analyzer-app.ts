// Interface for the Phase Noise Analyzer driver
// (c) Koheron

class PhaseNoiseAnalyzerApp {
  private disposed = false;
  private cicRateInput: HTMLInputElement;
  private nAvgInput: HTMLInputElement;
  private channelInputs: HTMLInputElement[];
  private carrierPowerSpan: HTMLElement;
  private phaseJitterSpan: HTMLElement;
  private timeJitterSpan: HTMLElement;

  private laserModeEnableCheckbox: HTMLInputElement;
  private interferometerDelayInput: HTMLInputElement;

  private ddsInputs: HTMLInputElement[];
  private ddsSetButtons: HTMLButtonElement[];
  private trackingEnabledInput: HTMLInputElement;

  private numbers: {[field: string]: DigitInput} = {};
  public nPoints: number;
  public channel: number;

  constructor(private document: Document, private driver: PhaseNoiseAnalyzer) {}

  dispose(): void { this.disposed = true; Object.keys(this.numbers).forEach(key => this.numbers[key].dispose()); }

  async init(): Promise<void> {
    const parameters = await this.driver.getParameters();
    if (this.disposed) { return; }
    const tracking = await this.driver.getTrackingParameters();
    if (this.disposed) { return; }
    this.nPoints = parameters.data_size;

    this.channelInputs = <HTMLInputElement[]><any>this.document.getElementsByClassName("channel-input");
    this.carrierPowerSpan = <HTMLElement>this.document.getElementsByClassName("carrier-power-span")[0];
    this.phaseJitterSpan = <HTMLElement>this.document.getElementsByClassName("phase-jitter-span")[0];
    this.timeJitterSpan = <HTMLElement>this.document.getElementsByClassName("time-jitter-span")[0];

    this.ddsInputs = [0, 1].map(i =>
      this.document.querySelector<HTMLInputElement>(`.dds-input${i}`)!);
    this.ddsSetButtons = [0, 1].map(i =>
      this.document.querySelector<HTMLButtonElement>(`.dds-set${i}`)!);

    this.initNumbers(parameters, tracking);
    this.trackingEnabledInput = this.document.querySelector('.tracking-enabled-input');
    this.trackingEnabledInput.checked = tracking.enabled;
    this.trackingEnabledInput.addEventListener('change', () =>
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
    const number = (input: HTMLInputElement, value: number, unitLabel: string, commit: (value: number) => void, read: (parameters: IParameters) => number) =>
      new NumberInput(input, {
        value, minimum: Number(input.min), maximum: Number(input.max), resolution: 1, integer: true, unitLabel,
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
      this.ddsSetButtons[channel].addEventListener('click', () => this.numbers['lo' + channel].commit());
    });
  }

  initChannelInput(): void {
    for (let i = 0; i < this.channelInputs.length; i++) {
      this.channelInputs[i].addEventListener('change', (event) => {
        this.channel = parseInt((<HTMLInputElement>event.currentTarget).value);
          this.driver[(<HTMLInputElement>event.currentTarget).dataset.command](this.channel);
      })
    }
  }

  initLaserMode(): void {
    this.laserModeEnableCheckbox = <HTMLInputElement>this.document.getElementsByClassName("laser-mode-input")[0];
    this.interferometerDelayInput = <HTMLInputElement>this.document.getElementsByClassName("interferometer-delay")[0];

    this.laserModeEnableCheckbox.addEventListener("change", () => {
      const enabled: 0 | 1 = this.laserModeEnableCheckbox.checked ? 1 : 0;
      this.driver.setAnalyzerMode(enabled);
      this.interferometerDelayInput.disabled = !enabled;
    });

  }

  private formatFrequency(freq: number): string {
    if (Number.isNaN(freq)) {
      return "---";
    }

    const absFreq = Math.abs(freq);

    if (absFreq >= 1e9) {
      return `${(freq / 1e9).toFixed(0)} GHz`;
    } else if (absFreq >= 1e6) {
      return `${(freq / 1e6).toFixed(0)} MHz`;
    } else if (absFreq >= 1e3) {
      return `${(freq / 1e3).toFixed(0)} kHz`;
    } else {
      return `${freq.toFixed(0)} Hz`;
    }
  }

  private formatMeasurement(value: number, unit: string, digits: number = 2): string {
    if (Number.isNaN(value)) {
      return "---";
    } else {
      return `${value.toFixed(digits)}  ${unit}`;
    }
  }

  private async updateMeasurements() {
    if (this.disposed) { return; }
    const navg: number = 400;
    const meas = await this.driver.getMeasurements(navg);
    if (this.disposed) { return; }

    this.carrierPowerSpan.innerHTML = this.formatMeasurement(meas.carrier_power, "dBm");
    const freqRange = `(${this.formatFrequency(meas.freq_lo)} - ${this.formatFrequency(meas.freq_hi)})`;

    this.phaseJitterSpan.innerHTML =
      this.formatMeasurement(meas.phase_jitter * 1E3, `mrad<sub>rms</sub> ${freqRange}`);
    this.timeJitterSpan.innerHTML =
      this.formatMeasurement(meas.time_jitter * 1E12, `ps<sub>rms</sub> ${freqRange}`);

    setTimeout(() => { this.updateMeasurements(); }, 250);
  }

  private async updateControls(): Promise<void> {
    if (this.disposed) { return; }
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
    const fixed = (value: number) => Number.isFinite(value) ? value.toFixed(3) : '---';
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

    setTimeout(() => { this.updateControls(); }, 500);
  }
}
