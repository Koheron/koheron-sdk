// Interface for the Phase Noise Analyzer driver
// (c) Koheron

class PhaseNoiseAnalyzerApp {
  private cicRateInput: HTMLInputElement;
  private nAvgInput: HTMLInputElement;
  private channelInputs: HTMLInputElement[];
  private carrierPowerSpan: HTMLElement;
  private phaseJitterSpan: HTMLElement;
  private timeJitterSpan: HTMLElement;
  private isEditingCic: boolean;
  private isEditingNavg: boolean;
  public nPoints: number;
  public channel: number;

  constructor(document: Document, private driver) {}

  async init(callback: () => void): Promise<void> {
    const parameters = await this.driver.getParameters();
    this.nPoints = parameters.data_size;

    this.channelInputs = <HTMLInputElement[]><any>document.getElementsByClassName("channel-input");
    this.carrierPowerSpan = <HTMLElement>document.getElementsByClassName("carrier-power-span")[0];
    this.phaseJitterSpan = <HTMLElement>document.getElementsByClassName("phase-jitter-span")[0];
    this.timeJitterSpan = <HTMLElement>document.getElementsByClassName("time-jitter-span")[0];

    this.initCicRateInput();
    this.initNavgInput();
    this.initChannelInput();
    this.updateMeasurements();
    this.updateControls();
    callback();
  }

  initCicRateInput(): void {
    this.cicRateInput = <HTMLInputElement>document.getElementsByClassName("cic-rate-input")[0];

    this.cicRateInput.addEventListener("focus", () => {
      this.isEditingCic = true;
    });

    this.cicRateInput.addEventListener("blur", () => {
      this.isEditingCic = false;
      this.updateControls();
    });

    let events = ['change', 'input'];
    for (let j = 0; j < events.length; j++) {
      this.cicRateInput.addEventListener(events[j], (event) => {
          let command = (<HTMLInputElement>event.currentTarget).dataset.command;
          let value = (<HTMLInputElement>event.currentTarget).value;
          this.driver[command](value);
      });
    }
  }

  initNavgInput(): void {
    this.nAvgInput = <HTMLInputElement>document.getElementsByClassName("plot-navg-input")[0];

    this.nAvgInput.addEventListener("focus", () => {
      this.isEditingNavg = true;
    });

    this.nAvgInput.addEventListener("blur", () => {
      this.isEditingNavg = false;
      this.updateControls();
    });

    let events = ['change', 'input'];
    for (let j = 0; j < events.length; j++) {
      this.nAvgInput.addEventListener(events[j], (event) => {
          let value = parseInt((<HTMLInputElement>event.currentTarget).value);
          this.setNavg(value);
      });
    }
  }

  initChannelInput(): void {
    for (let i = 0; i < this.channelInputs.length; i++) {
      this.channelInputs[i].addEventListener('change', (event) => {
        this.channel = parseInt((<HTMLInputElement>event.currentTarget).value);
          this.driver[(<HTMLInputElement>event.currentTarget).dataset.command](this.channel);
      })
    }
  }

  private setNavg(navg: number) {
    this.driver.setFFTNavg(navg);
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

  private updateMeasurements() {
    let navg: number = 400;
    this.driver.getCarrierPower(navg, async (power: number) => {
      this.carrierPowerSpan.innerHTML = this.formatMeasurement(power, "dBm");

      const jitters = await this.driver.getJitter();
      const freqRange = `(${this.formatFrequency(jitters.freq_lo)} - ${this.formatFrequency(jitters.freq_hi)})`;

      this.phaseJitterSpan.innerHTML =
        this.formatMeasurement(jitters.phase_jitter * 1E3, `mrad<sub>rms</sub> ${freqRange}`);
      this.timeJitterSpan.innerHTML =
        this.formatMeasurement(jitters.time_jitter * 1E12, `ps<sub>rms</sub> ${freqRange}`);

      requestAnimationFrame(() => { this.updateMeasurements(); });
    });
  }

  private async updateControls(): Promise<void> {
    const parameters = await this.driver.getParameters();

    if (parameters.channel == 0) {
      this.channelInputs[0].checked = true;
      this.channelInputs[1].checked = false;
    } else {
      this.channelInputs[0].checked = false;
      this.channelInputs[1].checked = true;
    }

    if (!this.isEditingCic) {
      this.cicRateInput.value = parameters.cic_rate.toString();
    }

    if (!this.isEditingNavg) {
      this.nAvgInput.value = parameters.fft_navg.toString();
    }

    requestAnimationFrame( () => { this.updateControls(); } )
  }
}
