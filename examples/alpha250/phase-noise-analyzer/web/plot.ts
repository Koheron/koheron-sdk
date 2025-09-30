// Plot widget
// (c) Koheron

class Plot {
  public n_pts: number;
  public plot: jquery.flot.plot;
  public plot_data: Array<Array<number>>;
  private samplingFrequency: number;
  private decadeValuesTable: HTMLTableElement;

  private laserPlotTypeInputs: HTMLInputElement[];
  private laserPlotType: 'phase' | 'frequency' = 'phase';

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

    this.initLaserPlotType();
  }

  initLaserPlotType(): void {
    this.laserPlotTypeInputs = Array.from(
      document.getElementsByClassName('laser-plot-type')
    ) as HTMLInputElement[];

    const syncPlotType = () => {
      const selected = this.laserPlotTypeInputs.find(i => i.checked);
      this.laserPlotType = (selected?.value as 'phase' | 'frequency') ?? 'phase';
    };

    this.laserPlotTypeInputs.forEach(input => {
      input.addEventListener('change', syncPlotType);
    });

    syncPlotType();
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

  getDecadeValues(): Array<Array<number>> {
    let fmin: number = this.plotBasics.x_min;
    let fmax: number = this.plotBasics.x_max;

    let freq_decades: number[] = [1E-1, 1E0, 1E1, 1E2, 1E3, 1E4, 1E5, 1E6, 1E7];
    let decade_values = [];

    for (const freq of freq_decades) {
      if (freq < fmin || freq > fmax) {
        continue;
      }

      const idxFloat = 2 * freq * this.n_pts / this.samplingFrequency + 1;
      const i0 = Math.floor(idxFloat);
      const i1 = Math.min(i0 + 1, this.plot_data.length - 1);

      const v0 = this.plot_data[i0]?.[1];
      const v1 = this.plot_data[i1]?.[1];

      let v: number;
      if (Number.isFinite(v0) && Number.isFinite(v1)) {
        // (Linear interpolation
        const f0 = this.plot_data[i0][0], f1 = this.plot_data[i1][0];
        const t = (freq - f0) / (f1 - f0);
        v = v0 + (v1 - v0) * t;
      } else {
        v = Number.isFinite(v0) ? v0 : Number.isFinite(v1) ? v1 : NaN;
      }

      decade_values.push([freq, v]);
    }

    return decade_values;
  }

  private setDecadeValuesTable(): void {
    const decade_values = this.getDecadeValues();

    this.decadeValuesTable.innerHTML = `
      <colgroup>
        <col style="width:250px">
        <col>
      </colgroup>
      <thead>
        <tr>
          <th>Carrier Offset Frequency</th>
          <th>Phase Noise</th>
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
      valueCell.innerHTML = Number.isFinite(value[1]) ? `${value[1].toFixed(2)} dBc/Hz` : '---';
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

      if (this.laserPlotType === 'phase') {
        for (let i = nstart - 1; i < N; i++) {
          plot[i][1] = DB10 * Math.log(phaseNoise[i]) + SHIFT; // rad²/Hz => dBc/Hz
        }
      } else { // Frequency noise
        for (let i = nstart - 1; i < N; i++) {
          // plot[i][1] = DB10 * (2 * Math.log(plot[i][0]) + Math.log(phaseNoise[i])) + SHIFT; // rad²/Hz => dBc/Hz
          plot[i][1] = 10 * Math.log10(phaseNoise[i]) + 20 * Math.log10(plot[i][0]); // dB Hz²/Hz
        }
      }

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
        }
      );
    } catch (err) {
      console.error('updatePlot error:', err);
      this._busy = false;
      setTimeout(() => requestAnimationFrame(() => this.updatePlot()), 500);
    }
  }
}
