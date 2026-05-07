// Plot widget
// (c) Koheron

class Plot {
  public n_pts: number;
  public plot: jquery.flot.plot;
  public plot_data: Array<Array<number>>;
  public smooth_plot_data: Array<Array<number>>;
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
    this.initSmoothedToggle();
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


  initSmoothedToggle(): void {
    this.showSmoothedInput = document.getElementById('show-smoothed-trace') as HTMLInputElement;

    if (!this.showSmoothedInput) {
      return;
    }

    if (typeof this.showSmoothedInput.checked !== 'boolean') {
      this.showSmoothedInput.checked = true;
    }
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
    const fmin = this.plotBasics.x_min;
    const fmax = this.plotBasics.x_max;

    const freqDecades = [1E-1, 1E0, 1E1, 1E2, 1E3, 1E4, 1E5, 1E6, 1E7];
    const decadeValues: Array<Array<number>> = [];

    // Total averaging span = 0.10 decade
    const halfWidthDecades = 0.05;
    const scale = Math.pow(10, halfWidthDecades);

    for (const freq of freqDecades) {
      if (freq < fmin || freq > fmax) {
        continue;
      }

      const fLow = freq / scale;
      const fHigh = freq * scale;

      let sumLinear = 0;
      let count = 0;

      for (let i = 0; i < this.plot_data.length; i++) {
        const f = this.plot_data[i][0];

        if (f < fLow) {
          continue;
        }

        if (f > fHigh) {
          break;
        }

        const db = this.plot_data[i][1];

        if (Number.isFinite(db)) {
          sumLinear += Math.pow(10, db / 10);
          count++;
        }
      }

      let value: number;

      if (count > 0) {
        value = 10 * Math.log10(sumLinear / count);
      } else {
        value = this.interpolateNearestValid(freq);
      }

      decadeValues.push([freq, value]);
    }

    return decadeValues;
  }

  private interpolateNearestValid(freq: number): number {
    const data = this.plot_data;
    const N = data.length;

    let i1 = 0;

    while (i1 < N && data[i1][0] < freq) {
      i1++;
    }

    let i0 = i1 - 1;

    while (i0 >= 0 && !Number.isFinite(data[i0][1])) {
      i0--;
    }

    while (i1 < N && !Number.isFinite(data[i1][1])) {
      i1++;
    }

    if (i0 >= 0 && i1 < N) {
      const f0 = data[i0][0];
      const f1 = data[i1][0];
      const v0 = data[i0][1];
      const v1 = data[i1][1];

      const t = (freq - f0) / (f1 - f0);
      return v0 + (v1 - v0) * t;
    }

    if (i0 >= 0) {
      return data[i0][1];
    }

    if (i1 < N) {
      return data[i1][1];
    }

    return NaN;
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

    if (!this.smooth_plot_data || this.smooth_plot_data.length !== this.n_pts) {
      this.smooth_plot_data = Array.from({ length: this.n_pts }, () => [0, NaN]);
    }

    for (let i = 0; i < this.n_pts; i++) {
      this.smooth_plot_data[i][0] = this.plot_data[i][0];
    }
  }

  private computeSmoothedPlot(nstart: number): void {
    const src = this.plot_data;
    const dst = this.smooth_plot_data;
    const N = this.n_pts;

    const halfWidthDecades = 0.05;
    const scale = Math.pow(10, halfWidthDecades);

    for (let i = 0; i < nstart - 1; i++) {
      dst[i][1] = NaN;
    }

    let j0 = nstart - 1;
    let j1 = nstart - 2;

    let sumLinear = 0;
    let cnt = 0;

    const add = (j: number) => {
      const db = src[j][1];
      if (Number.isFinite(db)) {
        sumLinear += Math.pow(10, db / 10);
        cnt++;
      }
    };

    const remove = (j: number) => {
      const db = src[j][1];
      if (Number.isFinite(db)) {
        sumLinear -= Math.pow(10, db / 10);
        cnt--;
      }
    };

    for (let i = nstart - 1; i < N; i++) {
      const fi = src[i][0];

      if (!Number.isFinite(fi) || fi <= 0) {
        dst[i][1] = NaN;
        continue;
      }

      const fMin = fi / scale;
      const fMax = fi * scale;

      while (j1 + 1 < N && src[j1 + 1][0] <= fMax) {
        j1++;
        add(j1);
      }

      while (j0 < N && src[j0][0] < fMin) {
        remove(j0);
        j0++;
      }

      dst[i][1] = cnt > 0
        ? 10 * Math.log10(sumLinear / cnt)
        : NaN;
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
      const ddsFreq = await app.dds.getDDSFreq(this.driver.parameters.channel);

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

      this.computeSmoothedPlot(nstart);
      this.setDecadeValuesTable();

      const smoothedVisible = this.showSmoothedInput ? this.showSmoothedInput.checked : true;

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
        smoothedVisible ? this.smooth_plot_data : undefined,
        `${this.yLabel} (smoothed)`
      );
    } catch (err) {
      console.error('updatePlot error:', err);
      this._busy = false;
      setTimeout(() => requestAnimationFrame(() => this.updatePlot()), 500);
    }
  }
}
