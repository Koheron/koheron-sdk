// Interface for the Phase Noise Analyzer driver
// (c) Koheron

class PhaseNoiseAnalyzerApp {
  private minFrequencyInput: HTMLInputElement;
  private nAvgInput: HTMLInputElement;
  private resetCumulativeAveragerBtn: HTMLButtonElement;
  private channelInputs: HTMLInputElement[];
  private carrierPowerSpan: HTMLElement;
  private phaseJitterSpan: HTMLElement;
  private timeJitterSpan: HTMLElement;

  private ddsInputs: HTMLInputElement[];
  private ddsSetButtons: HTMLButtonElement[];
  private trackingEnabledInput: HTMLInputElement;
  private trackingEffectiveBandwidthSpan: HTMLElement;
  private trackingCorrectionXSpan: HTMLElement;
  private trackingCorrectionYSpan: HTMLElement;

  private updatingControls = false;

  private isEditingMinFrequency: boolean;
  private isEditingNavg: boolean;
  private isEditingDdsInputs: boolean;
  public nPoints: number;
  public channel: number;

  constructor(document: Document, private driver: PhaseNoiseAnalyzer) {}

  async init(): Promise<void> {
    const parameters = await this.driver.getParameters();
    this.nPoints = parameters.data_size;

    this.channelInputs = <HTMLInputElement[]><any>document.getElementsByClassName("channel-input");
    this.carrierPowerSpan = <HTMLElement>document.getElementsByClassName("carrier-power-span")[0];
    this.phaseJitterSpan = <HTMLElement>document.getElementsByClassName("phase-jitter-span")[0];
    this.timeJitterSpan = <HTMLElement>document.getElementsByClassName("time-jitter-span")[0];

    this.ddsInputs = [0, 1, 2, 3].map(i =>
      document.querySelector<HTMLInputElement>(`.dds-input${i}`)!);
    this.ddsSetButtons = [0, 1, 2, 3].map(i =>
      document.querySelector<HTMLButtonElement>(`.dds-set${i}`)!);
    this.trackingEnabledInput = document.querySelector<HTMLInputElement>('.tracking-enabled-input')!;
    this.trackingEffectiveBandwidthSpan = document.querySelector<HTMLElement>('.tracking-effective-bandwidth')!;
    this.trackingCorrectionXSpan = document.querySelector<HTMLElement>('.tracking-correction-x')!;
    this.trackingCorrectionYSpan = document.querySelector<HTMLElement>('.tracking-correction-y')!;

    this.initMinFrequencyInput();
    this.initNavgInput();
    this.initChannelInput();
    this.updateMeasurements();
    this.updateControls();
  }

  initMinFrequencyInput(): void {
    this.minFrequencyInput = <HTMLInputElement>document.getElementsByClassName("min-frequency-input")[0];

    this.minFrequencyInput.addEventListener("focus", () => {
      this.isEditingMinFrequency = true;
    });

    this.minFrequencyInput.addEventListener("blur", () => {
      this.isEditingMinFrequency = false;
      this.updateControls();
    });

    let events = ['change'];
    for (let j = 0; j < events.length; j++) {
      this.minFrequencyInput.addEventListener(events[j], (event) => {
          let command = (<HTMLInputElement>event.currentTarget).dataset.command;
          let value = (<HTMLInputElement>event.currentTarget).value;
          const frequency = parseFloat(value);
          if (Number.isFinite(frequency) && frequency > 0) this.driver[command](frequency);
      });
    }

    const editingChannels = new Set<number>();
    for (let channel = 0; channel < this.ddsInputs.length; channel++) {
      const input = this.ddsInputs[channel];
      let dirty = false;
      let acceptedValue = input.value;
      const finishEditing = () => {
        editingChannels.delete(channel);
        this.isEditingDdsInputs = editingChannels.size > 0;
      };
      const commit = () => {
        if (!dirty) { finishEditing(); return; }
        const frequency = 1E6 * Number(input.value);
        if (input.value.trim() === '' || !input.checkValidity() ||
            !Number.isFinite(frequency) || frequency < 0 || frequency > 100E6) return;
        this.driver.setLocalOscillator(channel, frequency);
        acceptedValue = input.value;
        dirty = false;
        finishEditing();
        this.updateControls();
      };
      input.addEventListener('focus', () => {
        if (!dirty) acceptedValue = input.value;
        editingChannels.add(channel);
        this.isEditingDdsInputs = true;
      });
      input.addEventListener('input', () => {
        dirty = true;
        editingChannels.add(channel);
        this.isEditingDdsInputs = true;
      });
      input.addEventListener('blur', commit);
      input.addEventListener('keydown', (event: KeyboardEvent) => {
        if (event.key === 'Enter') { event.preventDefault(); commit(); }
        if (event.key === 'Escape') {
          event.preventDefault();
          input.value = acceptedValue;
          dirty = false;
          finishEditing();
          this.updateControls();
        }
      });
      this.ddsSetButtons[channel].addEventListener('click', commit);
    }

    this.trackingEnabledInput.addEventListener('change', (event) => {
      const enabled = (event.currentTarget as HTMLInputElement).checked;
      this.driver.setTrackingEnabled(enabled);
    });
  }

  initNavgInput(): void {
    this.nAvgInput = <HTMLInputElement>document.getElementsByClassName("plot-navg-input")[0];
    this.resetCumulativeAveragerBtn = <HTMLButtonElement>document.getElementsByClassName("reset-cumulative-averager-btn")[0];

    this.nAvgInput.addEventListener("focus", () => {
      this.isEditingNavg = true;
    });

    this.nAvgInput.addEventListener("blur", () => {
      this.isEditingNavg = false;
      this.updateControls();
    });

    let events = ['change'];
    for (let j = 0; j < events.length; j++) {
      this.nAvgInput.addEventListener(events[j], (event) => {
          let value = parseInt((<HTMLInputElement>event.currentTarget).value);
          if (Number.isFinite(value) && value >= 1 && value <= 200) this.setNavg(value);
      });
    }

    this.resetCumulativeAveragerBtn.addEventListener("click", () => {
      this.driver.resetCumulativeAverager();
      this.updateControls();
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

  private setNavg(navg: number) {
    this.driver.setFFTNavg(navg);
  }

  private formatFrequency(freq: number): string {
    if (!Number.isFinite(freq)) {
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
    if (!Number.isFinite(value)) {
      return "---";
    } else {
      return `${value.toFixed(digits)}  ${unit}`;
    }
  }

  private async updateMeasurements() {
    try {
      const navg: number = 400;
      const meas = await this.driver.getMeasurements(navg);

      this.carrierPowerSpan.innerHTML = this.formatMeasurement(meas.carrier_power, "dBm");
      const freqRange = `(${this.formatFrequency(meas.freq_lo)} - ${this.formatFrequency(meas.freq_hi)})`;

      this.phaseJitterSpan.innerHTML =
        this.formatMeasurement(meas.phase_jitter * 1E3, `mrad<sub>rms</sub> ${freqRange}`);
      this.timeJitterSpan.innerHTML =
        this.formatMeasurement(meas.time_jitter * 1E12, `ps<sub>rms</sub> ${freqRange}`);

    } catch (error) {
      console.error('updateMeasurements error:', error);
    } finally {
      setTimeout(() => { this.updateMeasurements(); }, 250);
    }
  }

  private async updateControls(): Promise<void> {
    if (this.updatingControls) return;
    this.updatingControls = true;
    try {
      const parameters = await this.driver.getParameters();
      const trackingParameters = await this.driver.getTrackingParameters();

      if (parameters.channel == 0) {
        this.channelInputs[0].checked = true;
        this.channelInputs[1].checked = false;
        this.channelInputs[2].checked = false;
      } else if (parameters.channel == 1) {
        this.channelInputs[0].checked = false;
        this.channelInputs[1].checked = true;
        this.channelInputs[2].checked = false;
      } else {
        this.channelInputs[0].checked = false;
        this.channelInputs[1].checked = false;
        this.channelInputs[2].checked = true;
      }

      if (!this.isEditingMinFrequency) {
        this.minFrequencyInput.value = parameters.min_freq.toFixed(2).toString();
      }

      if (!this.isEditingNavg) {
        if (parameters.channel < 2) {
          this.nAvgInput.value = parameters.fft_navg.toString();
          this.nAvgInput.readOnly = false;
          this.nAvgInput.disabled = false;
          this.resetCumulativeAveragerBtn.style.display = "none";
          this.resetCumulativeAveragerBtn.disabled = true;
        } else { // XY mode
          this.nAvgInput.value = parameters.avgxy_count.toString();
          this.nAvgInput.readOnly = true;
          this.nAvgInput.disabled = true;
          this.resetCumulativeAveragerBtn.style.display = "";
          this.resetCumulativeAveragerBtn.disabled = false;
        }
      }

      if (!this.isEditingDdsInputs) {
        this.ddsInputs[0].value = (parameters.fdds0 / 1E6).toFixed(9);
        this.ddsInputs[1].value = (parameters.fdds1 / 1E6).toFixed(9);
        this.ddsInputs[2].value = (parameters.fdds2 / 1E6).toFixed(9);
        this.ddsInputs[3].value = (parameters.fdds3 / 1E6).toFixed(9);
      }

      this.trackingEnabledInput.checked = trackingParameters.tracking_enabled;
      this.trackingEffectiveBandwidthSpan.textContent = trackingParameters.effective_tracking_bandwidth.toFixed(6);
      this.trackingCorrectionXSpan.textContent = trackingParameters.tracking_correction_x.toFixed(6);
      this.trackingCorrectionYSpan.textContent = trackingParameters.tracking_correction_y.toFixed(6);

      (<HTMLInputElement>document.querySelector("[data-command='setReferenceClock'][value='" + parameters.clkIndex + "']")).checked = true;

    } catch (error) {
      console.error('updateControls error:', error);
    } finally {
      // Keep the guard set while waiting so edits cannot create another loop.
      setTimeout(() => {
        this.updatingControls = false;
        this.updateControls();
      }, 250);
    }
  }
}
