// Plot widget
// (c) Koheron

class Plot {
  public n_pts: number;
  public plot: jquery.flot.plot;
  public plot_data: Array<Array<number>>;
  public decade_values: Array<Array<number>>;
  private samplingFrequency: number;
  private decadeValuesTable: HTMLTableElement;

  public yLabel: string = "PHASE NOISE (dBc/Hz)";
  private peakDatapoint: number[];

  constructor(document: Document, private driver: PhaseNoiseAnalyzer, public plotBasics: PlotBasics) {
    this.peakDatapoint = [];
    this.plot_data = [];
    this.init();
    this.decadeValuesTable = <HTMLTableElement>document.getElementById('decade-values-table');
    this.updatePlot();
  }

  async init() {
    const parameters = await this.driver.getParameters();
    this.n_pts = parameters.data_size;
    this.samplingFrequency = parameters.fs;
    this.setFreqAxis();
    this.plotBasics.setLogX();
    this.plotBasics.enableDecimation();
  }

  setFreqAxis(): void {
    this.ensurePlotBuffer();
    const binWidth = this.samplingFrequency / (2 * this.n_pts);
    let x = -binWidth;

    for (let i = 0; i < this.n_pts; i++) {
      this.plot_data[i][0] = x;
      x += binWidth;
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

  getDecadeValues() {
    let fmin: number = this.plotBasics.x_min;
    let fmax: number = this.plotBasics.x_max;

    let freq_decades: number[] = [1E-1, 1E0, 1E1, 1E2, 1E3, 1E4, 1E5, 1E6, 1E7];
    this.decade_values = [];

    for (let freq of freq_decades) {
      if (freq >= fmin && freq <= fmax) {
        let i: number = Math.floor(2 * freq * this.n_pts / this.samplingFrequency) + 1;
        this.decade_values.push(this.plot_data[i]);
      }
    }

    this.decadeValuesTable.innerHTML = '<thead><tr><th>Carrier Offset Frequency</th><th>Phase Noise</th></tr></thead>';

    for (let value of this.decade_values) {
      let row = this.decadeValuesTable.insertRow(-1);
      let freqCell = row.insertCell(0);
      freqCell.innerHTML = this.frequencyFormater(value[0]);
      let valueCell = row.insertCell(1);

      if (Number.isFinite(value[1])) {
        valueCell.innerHTML = value[1].toFixed(2) + " dBc/Hz";
      } else {
        valueCell.innerHTML = "---";
      }
    }
  }

  private loIsSet(freqDdsHz: number): boolean {
    return Number.isFinite(freqDdsHz) && Math.abs(freqDdsHz) >= 1;
  }

  private _busy = false;
  private _targetHz = 60; // plot update cadence
  private _lastTick = 0;
  private _loBackoffMs = 250; // slower polling when LO is off

  private ensurePlotBuffer() {
    if (!this.plot_data || this.plot_data.length !== this.n_pts) {
      this.plot_data = Array.from({ length: this.n_pts }, () => [0, NaN]);
    }
  }

  async updatePlot() {
    if (this._busy) {
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
      const parameters = await this.driver.getParameters();

      const ddsFreq = await app.dds.getDDSFreq(parameters.channel);
      const plotEmptyDiv: HTMLElement = document.getElementById('plot-empty')!;

      if (!this.loIsSet(ddsFreq)) {
        plotEmptyDiv.classList.remove('hidden');
        this._busy = false;
        setTimeout(() => requestAnimationFrame(() => this.updatePlot()), this._loBackoffMs);
        return;
      } else {
        plotEmptyDiv.classList.add('hidden');
      }

      if (parameters.fs !== this.samplingFrequency) {
        this.samplingFrequency = parameters.fs;
        this.setFreqAxis();
      }

      const phaseNoise: Float32Array = await this.driver.getPhaseNoise();
      this.ensurePlotBuffer();

      const plot = this.plot_data;
      const N = this.n_pts;
      const nstart = 3;
      const DB10 = 10 * Math.LOG10E;
      const SHIFT = DB10 * Math.log(0.5);

      for (let i = 0; i < nstart - 1; i++) {
        plot[i][1] = NaN;
      }

      for (let i = nstart - 1; i < N; i++) {
        plot[i][1] = DB10 * Math.log(phaseNoise[i]) + SHIFT; // rad²/Hz => dBc/Hz
      }

      this.getDecadeValues();

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
        }
      );
    } catch (err) {
      console.error('updatePlot error:', err);
      this._busy = false;
      setTimeout(() => requestAnimationFrame(() => this.updatePlot()), 500);
    }
  }
}
