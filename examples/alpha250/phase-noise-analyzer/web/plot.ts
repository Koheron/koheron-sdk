// Plot widget
// (c) Koheron

class Plot {
  private disposed = false;
  public n_pts: number;
  public plot: jquery.flot.plot;
  public plot_data: Array<Array<number>>;
  public phase_psd: Float32Array = new Float32Array(0);
  public smooth_plot_data: Array<Array<number>>;
  private linear_plot_data: Array<Array<number>>;
  private samplingFrequency: number;
  private decadeValuesTable: HTMLTableElement;
  private frameParameters: IParameters;
  public frameReceivedAt: string;
  private reference: {psd: Float32Array; parameters: IParameters; receivedAt: string};
  private hasInitialFit = false;
  private referencePlotType: string;
  public reference_data: number[][];
  public reference_smooth_data: number[][];
  public get referenceParameters(): IParameters { return this.reference?.parameters; }
  public get referencePSD(): Float32Array { return this.reference?.psd; }
  public get referenceReceivedAt(): string { return this.reference?.receivedAt; }
  public get frameStatus(): IParameters { return this.frameParameters; }

  private laserPlotTypeInputs: HTMLInputElement[];
  private showSmoothedInput: HTMLInputElement;
  private laserPlotType: 'phase' | 'frequency' = 'phase';

  public yLabel: string = 'Phase noise (dBc/Hz)';
  private peakDatapoint: number[];

  constructor(private document: Document, private driver: PhaseNoiseAnalyzer, public plotBasics: PlotBasics,
      private onConnectionError: (error: unknown) => void = () => {}) {
    this.peakDatapoint = [];
    this.plot_data = [];
    this.smooth_plot_data = [];
    this.linear_plot_data = [];
    this.init();
    this.decadeValuesTable = <HTMLTableElement>document.getElementById('decade-values-table');
    this.document.addEventListener('visibilitychange', this.visibilityHandler);
    this.updatePlot();
  }

  init() {
    this.n_pts = this.driver.parameters.data_size;
    this.samplingFrequency = this.driver.parameters.fs;
    this.setFreqAxis();
    this.plotBasics.setLogX(true);
    this.plotBasics.enableDecimation();
    this.plotBasics.setPrimaryTraceLabel('Raw');

    this.initLaserPlotType();
    this.showSmoothedInput = document.getElementById('show-smoothed-trace') as HTMLInputElement;
    this.showSmoothedInput?.addEventListener('change', () => {
      this.plotBasics.refreshLegend();
      if (this.frameParameters) { this.redraw(() => {}); }
    });
    document.getElementById('capture-reference')?.addEventListener('click', () => this.captureReference());
    document.getElementById('clear-reference')?.addEventListener('click', () => this.clearReference());
    document.getElementById('fit-view')?.addEventListener('click', () => {
      this.plotBasics.setRangeX(this.plotBasics.x_min, this.plotBasics.x_max);
      this.plotBasics.setLinY();
      if (this.frameParameters) { this.redraw(() => {}); }
    });
  }

  initLaserPlotType(): void {
    this.laserPlotTypeInputs = Array.from(
      document.getElementsByClassName('laser-plot-type')
    ) as HTMLInputElement[];

    const syncPlotType = () => {
      const selected = this.laserPlotTypeInputs.find(i => i.checked);
      const nextType = (selected?.value as 'phase' | 'frequency') ?? 'phase';
      if (nextType !== this.laserPlotType) {
        this.laserPlotType = nextType;
        // Rebuild the legend and fit the new units while retaining the X zoom.
        this.plotBasics.setLinY();
      }
      this.yLabel = this.laserPlotType === 'phase' ? 'Phase noise (dBc/Hz)' : 'Frequency noise (dB Hz²/Hz)';
      const unit = document.getElementById('spectrum-unit');
      if (unit) { unit.textContent = this.laserPlotType === 'phase' ? 'dBc/Hz' : 'dB Hz²/Hz'; }
      if (this.frameParameters) {
        this.computeDisplaySpectrum(this.phase_psd, 2);
        this.computeSmoothedPlot(2);
        this.updateReferenceDisplay();
        this.setDecadeValuesTable();
        this.redraw(() => {});
      }
    };

    this.laserPlotTypeInputs.forEach(input => {
      input.addEventListener('change', syncPlotType);
    });

    syncPlotType();
  }

  setFreqAxis(): void {
    this.ensurePlotBuffer();
    const binWidth = this.samplingFrequency / (2 * (this.n_pts - 1));

    for (let i = 0; i < this.n_pts; i++) {
      this.plot_data[i][0] = i * binWidth;
      this.linear_plot_data[i][0] = i * binWidth;
      this.smooth_plot_data[i][0] = i * binWidth;
    }

    this.plotBasics.x_min = 2 * binWidth;
    this.plotBasics.x_max = 0.75 * 0.5 * this.samplingFrequency;
    this.plotBasics.setRangeX(this.plotBasics.x_min, this.plotBasics.x_max);
  }

  frequencyFormater(val: number) {
    if (val >= 1E6) {
      return (val / 1E6).toString() + " MHz";
    } else if (val >= 1E3) {
        return (val / 1E3).toFixed() + " kHz";
    } else {
        return val.toFixed() + " Hz";
    }
  }

  getDecadeValues(): Array<Array<number>> {
    let fmin: number = this.plotBasics.x_min;
    let fmax: number = this.plotBasics.x_max;

    let freq_decades: number[] = [1E-1, 1E0, 1E1, 1E2, 1E3, 1E4, 1E5, 1E6, 1E7];
    let decade_values = [];

    for (const freq of freq_decades) {
      if (freq < fmin || freq > fmax) {
        continue;
      }

      const scale = Math.pow(10, 0.05);
      const binsPerHz = 2 * (this.n_pts - 1) / this.samplingFrequency;
      const i0 = Math.max(2, Math.ceil(freq / scale * binsPerHz));
      const i1 = Math.min(this.n_pts - 1, Math.floor(freq * scale * binsPerHz));
      let sum = 0, count = 0;
      for (let i = i0; i <= i1; i++) {
        const value = this.linear_plot_data[i][1];
        if (Number.isFinite(value)) { sum += value; count++; }
      }
      decade_values.push([freq, count > 0 && sum > 0 ? 10 * Math.log10(sum / count) : NaN]);
    }

    return decade_values;
  }

  private setDecadeValuesTable(): void {
    const decade_values = this.getDecadeValues();

    this.decadeValuesTable.innerHTML = `
      <colgroup>
        <col>
        <col>
      </colgroup>
      <thead>
        <tr>
          <th>Offset</th>
          <th>${this.laserPlotType === 'phase' ? 'Phase noise' : 'Frequency noise'}</th>
        </tr>
      </thead>
      <tbody></tbody>
    `;

    const tbody = this.decadeValuesTable.tBodies[0] as HTMLTableSectionElement;

    for (const value of decade_values) {
      const row = tbody.insertRow(-1);
      const freqCell = row.insertCell(0);
      freqCell.innerHTML = this.frequencyFormater(value[0]);

      const valueCell = row.insertCell(1);
      const unit = this.laserPlotType === 'phase' ? 'dBc/Hz' : 'dB Hz²/Hz';
      valueCell.innerHTML = Number.isFinite(value[1]) ? `${value[1].toFixed(2)} ${unit}` : '—';
    }
  }

  private loIsSet(freqDdsHz: number): boolean {
    return Number.isFinite(freqDdsHz) && Math.abs(freqDdsHz) >= 1;
  }

  private _busy = false;
  private _targetHz = 60; // displayed spectrum updates per second
  private _lastTick = -Infinity;
  private timer: number;
  private rateStarted = performance.now();
  private displayedFrames = 0;
  private lastTableUpdate = -Infinity;
  private visibilityHandler = () => {
    window.clearTimeout(this.timer);
    this.resetRate();
    if (!this.document.hidden) { void this.updatePlot(); }
  };
  private _loBackoffMs = 250; // slower polling when LO is off

  private ensurePlotBuffer() {
    if (!this.plot_data || this.plot_data.length !== this.n_pts) {
      this.plot_data = Array.from({ length: this.n_pts }, () => [0, NaN]);
    }
    if (!this.linear_plot_data || this.linear_plot_data.length !== this.n_pts) {
      this.linear_plot_data = Array.from({ length: this.n_pts }, () => [0, NaN]);
    }
    if (!this.smooth_plot_data || this.smooth_plot_data.length !== this.n_pts) {
      this.smooth_plot_data = Array.from({ length: this.n_pts }, () => [0, NaN]);
    }
  }

  private computeDisplaySpectrum(phaseNoise: Float32Array, firstBin: number,
      data = this.plot_data, linear = this.linear_plot_data): void {
    for (let i = 0; i < phaseNoise.length; i++) {
      const f = data[i][0];
      const value = phaseNoise[i] * (this.laserPlotType === 'phase' ? 0.5 : f * f);
      linear[i][1] = i >= firstBin && Number.isFinite(value) && value >= 0 ? value : NaN;
      data[i][1] = i >= firstBin && Number.isFinite(value) && value > 0
        ? 10 * Math.log10(value) : NaN;
    }
  }

  private computeSmoothedPlot(firstBin: number, src = this.linear_plot_data, dst = this.smooth_plot_data): void {
    // ALPHA250-4 display smoothing: average linear density in a 0.1-decade
    // frequency window, then convert to dB. Keep raw data and jitter unchanged.
    const scale = Math.pow(10, 0.05);
    let j0 = firstBin, j1 = firstBin - 1, sum = 0, count = 0;
    for (let i = 0; i < firstBin; i++) dst[i][1] = NaN;
    for (let i = firstBin; i < src.length; i++) {
      const f = src[i][0];
      while (j1 + 1 < src.length && src[j1 + 1][0] <= f * scale) {
        const value = src[++j1][1];
        if (Number.isFinite(value)) { sum += value; count++; }
      }
      while (j0 <= j1 && src[j0][0] < f / scale) {
        const value = src[j0++][1];
        if (Number.isFinite(value)) { sum -= value; count--; }
      }
      const mean = count > 0 ? sum / count : NaN;
      dst[i][1] = Number.isFinite(mean) && mean > 0 ? 10 * Math.log10(mean) : NaN;
    }
  }

  captureReference(): void {
    if (!this.frameParameters || !this.phase_psd.subarray(2).some(v => Number.isFinite(v) && v > 0)) { return; }
    // Same snapshot model as the FFT workspace: retain linear density and the
    // displayed frame's settings, so later rate/unit changes cannot alter it.
    this.reference = {psd: this.phase_psd.slice(), parameters: {...this.frameParameters}, receivedAt: this.frameReceivedAt};
    this.reference_data = undefined;
    document.getElementById('capture-reference').textContent = 'Replace ref';
    (document.getElementById('clear-reference') as HTMLButtonElement).disabled = false;
    document.getElementById('reference-info').hidden = false;
    const p = this.reference.parameters;
    document.getElementById('reference-status').textContent =
      `ADC ${p.channel} · CIC ${p.cic_rate} · N ${p.fft_navg} · ${p.analyzer_mode.toUpperCase()}`;
    document.getElementById('reference-info').title = `Captured ${new Date(this.reference.receivedAt).toLocaleString()}`;
    this.updateReferenceDisplay();
    this.plotBasics.refreshLegend();
    this.redraw(() => {});
  }

  clearReference(): void {
    this.reference = undefined;
    this.reference_data = this.reference_smooth_data = undefined;
    document.getElementById('capture-reference').textContent = 'Capture ref';
    (document.getElementById('clear-reference') as HTMLButtonElement).disabled = true;
    document.getElementById('reference-info').hidden = true;
    this.plotBasics.refreshLegend();
    this.redraw(() => {});
  }

  private updateReferenceDisplay(): void {
    if (!this.reference || (this.reference_data && this.referencePlotType === this.laserPlotType)) { return; }
    const {psd, parameters} = this.reference;
    const binWidth = parameters.fs / (2 * (psd.length - 1));
    this.reference_data = Array.from(psd, (_, i) => [i * binWidth, NaN]);
    this.reference_smooth_data = this.reference_data.map(row => row.slice());
    const linear = this.reference_data.map(row => row.slice());
    this.computeDisplaySpectrum(psd, 2, this.reference_data, linear);
    this.computeSmoothedPlot(2, linear, this.reference_smooth_data);
    this.referencePlotType = this.laserPlotType;
  }

  private setCaptureReady(ready: boolean): void {
    const button = document.getElementById('capture-reference') as HTMLButtonElement;
    if (button) { button.disabled = !ready; }
    const fit = document.getElementById('fit-view') as HTMLButtonElement;
    if (fit) { fit.disabled = !ready; }
    document.querySelectorAll<HTMLButtonElement>('.export-data, .export-plot').forEach(button => {
      button.disabled = !ready;
      button.title = ready ? (button.classList.contains('export-data') ? 'Export the displayed spectrum as CSV' : 'Export the noise plot as PNG')
        : 'Waiting for a valid live spectrum';
    });
    if (!ready) { this.frameParameters = undefined; }
  }

  public showUnavailable(title: string, message: string): void {
    this.setCaptureReady(false);
    this.resetRate();
    const empty = document.getElementById('plot-empty');
    if (!empty) { return; }
    empty.classList.remove('hidden');
    const heading = empty.querySelector('h4');
    const detail = empty.querySelector('p');
    if (heading) { heading.textContent = title; }
    if (detail) { detail.textContent = message; }
  }

  private redraw(callback: () => void): void {
    if (!this.plot_data.length) { return; }
    this.plotBasics.redraw(this.plot_data, this.n_pts, this.peakDatapoint, this.yLabel, callback,
      this.reference ? (this.showSmoothedInput?.checked ? this.reference_smooth_data : this.reference_data) : undefined,
      false,
      this.showSmoothedInput?.checked ? [{label: 'Smoothed', data: this.smooth_plot_data, color: '#006400'}] : []);
  }

  async updatePlot() {
    if (this.disposed || this.document.hidden || this._busy) {
      return;
    }

    this._busy = true;

    const frameBudgetMs = 1000 / this._targetHz;
    const now = performance.now();
    const sinceLast = now - this._lastTick;

    // Throttle to targetHz
    if (sinceLast < frameBudgetMs) {
      this._busy = false;
      this.schedule(frameBudgetMs - sinceLast);
      return;
    }

    this._lastTick = now;

    try {
      // The controls already refresh these parameters and read back LO edits.
      // Avoid an extra RPC round trip for every displayed spectrum.
      const parameters = this.driver.parameters;
      const ddsFreq = parameters.channel === 0 ? parameters.fdds0 : parameters.fdds1;

      const plotEmptyDiv: HTMLElement = document.getElementById('plot-empty')!;

      if (!this.loIsSet(ddsFreq)) {
        this.showUnavailable('Local oscillator not set', 'Set the local oscillator frequency to start measurement.');
        this._busy = false;
        this.schedule(this._loBackoffMs);
        return;
      }

      if (this.driver.parameters.fs !== this.samplingFrequency) {
        this.samplingFrequency = this.driver.parameters.fs;
        this.setFreqAxis();
      }

      const frameParameters = {...this.driver.parameters};
      const phaseNoise: Float32Array = await this.driver.getPhaseNoise();
      if (this.disposed) { return; }
      this.phase_psd = phaseNoise;
      if (this.n_pts !== phaseNoise.length) {
        this.n_pts = phaseNoise.length;
        this.setFreqAxis();
      }
      this.ensurePlotBuffer();

      this.computeDisplaySpectrum(phaseNoise, 2);
      this.computeSmoothedPlot(2);
      this.frameParameters = {...frameParameters, fs: this.samplingFrequency, data_size: phaseNoise.length};
      this.frameReceivedAt = new Date().toISOString();
      const ready = phaseNoise.subarray(2).some(v => Number.isFinite(v) && v > 0);
      this.setCaptureReady(ready);
      if (ready) {
        plotEmptyDiv.classList.add('hidden');
        if (!this.hasInitialFit) {
          this.plotBasics.setLinY();
          this.hasInitialFit = true;
        }
      } else {
        this.showUnavailable('Waiting for valid spectrum', 'Acquisition is settling. The plot will resume automatically.');
      }
      this.updateReferenceDisplay();

      // Numeric readouts need a lower cadence than the spectrum animation.
      if (now - this.lastTableUpdate >= 250) {
        this.setDecadeValuesTable();
        this.lastTableUpdate = now;
      }

      this.redraw(() => {
          this._busy = false;
          if (this.disposed) { return; }
          if (ready) { this.recordFrame(); }
          const elapsed = performance.now() - now;
          this.schedule(Math.max(0, frameBudgetMs - elapsed));
        });
    } catch (err) {
      if (this.disposed) { return; }
      console.error('updatePlot error:', err);
      this.showUnavailable('Measurement unavailable', 'Unable to read the live spectrum.');
      this.onConnectionError?.(err);
      this._busy = false;
      this.schedule(500);
    }
  }

  private schedule(delay: number): void {
    window.clearTimeout(this.timer);
    if (this.disposed || this.document.hidden) { return; }
    // A timer followed by requestAnimationFrame adds a second wait and can
    // halve the cadence on a 60 Hz screen. Keep one serial polling loop.
    this.timer = window.setTimeout(() => { void this.updatePlot(); }, delay);
  }

  private resetRate(): void {
    this.rateStarted = performance.now();
    this.displayedFrames = 0;
    const rate = this.document.getElementById('refresh-rate');
    if (rate) { rate.textContent = '— FPS'; }
  }

  private recordFrame(): void {
    if (this.document.hidden) { return; }
    this.displayedFrames++;
    const elapsed = performance.now() - this.rateStarted;
    if (elapsed < 1000) { return; }
    const rate = this.document.getElementById('refresh-rate');
    if (rate) {
      rate.textContent = (this.displayedFrames * 1000 / elapsed).toFixed(0) + ' FPS';
      rate.title = `Displayed spectrum updates per second; target ${this._targetHz} FPS`;
    }
    this.rateStarted = performance.now();
    this.displayedFrames = 0;
  }

  dispose(): void {
    this.disposed = true;
    window.clearTimeout(this.timer);
    this.document.removeEventListener('visibilitychange', this.visibilityHandler);
    this.resetRate();
  }
}
