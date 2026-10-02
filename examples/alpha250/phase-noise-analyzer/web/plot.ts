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

  private laserPlotTypeInputs: HTMLInputElement[];
  private showSmoothedInput: HTMLInputElement;
  private laserPlotType: 'phase' | 'frequency' = 'phase';

  public yLabel: string = "PHASE NOISE (dBc/Hz)";
  private peakDatapoint: number[];

  constructor(document: Document, private driver: PhaseNoiseAnalyzer, public plotBasics: PlotBasics) {
    this.peakDatapoint = [];
    this.plot_data = [];
    this.smooth_plot_data = [];
    this.linear_plot_data = [];
    this.init();
    this.decadeValuesTable = <HTMLTableElement>document.getElementById('decade-values-table');
    this.updatePlot();
  }

  init() {
    this.n_pts = this.driver.parameters.data_size;
    this.samplingFrequency = this.driver.parameters.fs;
    this.setFreqAxis();
    this.plotBasics.setLogX();
    this.plotBasics.enableDecimation();

    this.initLaserPlotType();
    this.showSmoothedInput = document.getElementById('show-smoothed-trace') as HTMLInputElement;
    this.showSmoothedInput?.addEventListener('change', () => this.plotBasics.refreshLegend());
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
      this.yLabel = this.laserPlotType === 'phase' ? 'PHASE NOISE (dBc/Hz)' : 'FREQUENCY NOISE (dB Hz²/Hz)';
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
      valueCell.innerHTML = Number.isFinite(value[1]) ? `${value[1].toFixed(2)} ${unit}` : '---';
    }
  }

  private loIsSet(freqDdsHz: number): boolean {
    return Number.isFinite(freqDdsHz) && Math.abs(freqDdsHz) >= 1;
  }

  private _busy = false;
  private _targetHz = 10; // plot update cadence
  private _lastTick = 0;
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

  private computeDisplaySpectrum(phaseNoise: Float32Array, firstBin: number): void {
    for (let i = 0; i < this.n_pts; i++) {
      const f = this.plot_data[i][0];
      const value = phaseNoise[i] * (this.laserPlotType === 'phase' ? 0.5 : f * f);
      this.linear_plot_data[i][1] = i >= firstBin && Number.isFinite(value) && value >= 0 ? value : NaN;
      this.plot_data[i][1] = i >= firstBin && Number.isFinite(value) && value > 0
        ? 10 * Math.log10(value) : NaN;
    }
  }

  private computeSmoothedPlot(firstBin: number): void {
    // ALPHA250-4 display smoothing: average linear density in a 0.1-decade
    // frequency window, then convert to dB. Keep raw data and jitter unchanged.
    const src = this.linear_plot_data;
    const dst = this.smooth_plot_data;
    const scale = Math.pow(10, 0.05);
    let j0 = firstBin, j1 = firstBin - 1, sum = 0, count = 0;
    for (let i = 0; i < firstBin; i++) dst[i][1] = NaN;
    for (let i = firstBin; i < this.n_pts; i++) {
      const f = src[i][0];
      while (j1 + 1 < this.n_pts && src[j1 + 1][0] <= f * scale) {
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

  async updatePlot() {
    if (this.disposed || this._busy) {
      return;
    }

    this._busy = true;

    const frameBudgetMs = 1000 / this._targetHz;
    const now = performance.now();
    const sinceLast = now - this._lastTick;

    // Throttle to targetHz
    if (sinceLast < frameBudgetMs) {
      this._busy = false;
      setTimeout(() => requestAnimationFrame(() => this.updatePlot()), Math.ceil(frameBudgetMs - sinceLast));
      return;
    }

    this._lastTick = now;

    try {
      const ddsFreq = await app.dds.getDDSFreq(this.driver.parameters.channel);
      if (this.disposed) { return; }

      const plotEmptyDiv: HTMLElement = document.getElementById('plot-empty')!;

      if (!this.loIsSet(ddsFreq)) {
        plotEmptyDiv.classList.remove('hidden');
        this._busy = false;
        setTimeout(() => requestAnimationFrame(() => this.updatePlot()), this._loBackoffMs);
        return;
      } else {
        plotEmptyDiv.classList.add('hidden');
      }

      if (this.driver.parameters.fs !== this.samplingFrequency) {
        this.samplingFrequency = this.driver.parameters.fs;
        this.setFreqAxis();
      }

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

      this.setDecadeValuesTable();

      this.plotBasics.redraw(
        this.plot_data,
        this.n_pts,
        this.peakDatapoint,
        this.yLabel,
        () => {
          this._busy = false;
          const elapsed = performance.now() - now;
          const delay = Math.max(0, Math.ceil(frameBudgetMs - elapsed));
          setTimeout(() => requestAnimationFrame(() => this.updatePlot()), delay);
        },
        this.showSmoothedInput?.checked ? this.smooth_plot_data : undefined,
        false,
        [],
        `${this.yLabel} (smoothed)`
      );
    } catch (err) {
      if (this.disposed) { return; }
      console.error('updatePlot error:', err);
      this._busy = false;
      setTimeout(() => requestAnimationFrame(() => this.updatePlot()), 500);
    }
  }

  dispose(): void { this.disposed = true; }
}
