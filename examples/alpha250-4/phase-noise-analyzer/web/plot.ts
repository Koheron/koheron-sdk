// Plot widget
// (c) Koheron

class Plot {
  public n_pts: number;
  public plot: jquery.flot.plot;
  public plot_data: Array<Array<number>>;
  public signed_phase_psd: Float32Array = new Float32Array(0);
  public smooth_plot_data: Array<Array<number>>;
  private negative_plot_data: number[][] = [];
  private negative_smooth_data: number[][] = [];
  private linear_plot_data: Array<Array<number>>;
  private samplingFrequency: number;
  private decadeValuesTable: HTMLTableElement;

  private laserPlotTypeInputs: HTMLInputElement[];
  private showSmoothedInput: HTMLInputElement;
  private laserPlotType: 'phase' | 'frequency' = 'phase';
  private displayedChannel: number;

  public yLabel: string = "PHASE NOISE MAGNITUDE (dBc/Hz)";
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
    this.n_pts = 0;
    this.samplingFrequency = this.driver.parameters.fs;
    this.setFreqAxis();
    this.plotBasics.setLogX();
    this.plotBasics.setLinY(); // Autoscale the first spectrum, then retain user zoom.
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
      const nextType = (selected?.value as 'phase' | 'frequency') ?? 'phase';
      if (nextType !== this.laserPlotType) {
        this.laserPlotType = nextType;
        // Flot rebuilds its legend on setupGrid, rather than setData/draw.
        // Recompute the Y range when changing units and retain the X range.
        this.plotBasics.setLinY();
      }
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

    this.showSmoothedInput.addEventListener('change', () => {
      // Rebuild the legend without resetting the user's axis ranges.
      this.plotBasics.refreshLegend();
    });
  }

  setFreqAxis(): void {
    this.ensurePlotBuffer();

    const fftSize = 2 * (this.n_pts - 1);
    const df = this.samplingFrequency / fftSize;

    for (let i = 0; i < this.n_pts; i++) {
      this.plot_data[i][0] = i * df;
      this.linear_plot_data[i][0] = i * df;
      this.smooth_plot_data[i][0] = i * df;
    }

    this.plotBasics.x_min = 2 * df;
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

        const value = this.linear_plot_data[i][1];

        if (Number.isFinite(value)) {
          sumLinear += value;
          count++;
        }
      }

      let value: number;

      value = count > 0 && sumLinear > 0 ? 10 * Math.log10(sumLinear / count) : NaN;

      decadeValues.push([freq, value]);
    }

    return decadeValues;
  }

  private setDecadeValuesTable(): void {
    const decade_values = this.getDecadeValues();

    this.decadeValuesTable.innerHTML = `
      <colgroup>
        <col style="width:50%">
        <col>
      </colgroup>
      <thead>
        <tr>
          <th>Carrier Offset Frequency</th>
          <th>${this.laserPlotType === 'phase' ? 'Phase Noise' : 'Frequency Noise'}</th>
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
      valueCell.innerHTML = Number.isFinite(value[1]) ? `${value[1].toFixed(2)} ${this.laserPlotType === 'phase' ? 'dBc/Hz' : 'dB Hz²/Hz'}` : 'Nonpositive estimate';
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

    for (let i = 0; i < this.n_pts; i++) {
      this.smooth_plot_data[i][0] = this.plot_data[i][0];
    }
  }

  private computeSmoothedPlot(nstart: number): void {
    const src = this.linear_plot_data;
    const dst = this.smooth_plot_data;
    const N = this.n_pts;
    this.negative_smooth_data = [];

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
      const value = src[j][1];
      if (Number.isFinite(value)) {
        sumLinear += value;
        cnt++;
      }
    };

    const remove = (j: number) => {
      const value = src[j][1];
      if (Number.isFinite(value)) {
        sumLinear -= value;
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

      // Average signed estimates BEFORE taking the magnitude for display.
      const mean = cnt > 0 ? sumLinear / cnt : NaN;
      dst[i][1] = Number.isFinite(mean) && mean !== 0
        ? 10 * Math.log10(Math.abs(mean)) : NaN;
      if (mean < 0) this.negative_smooth_data.push([fi, dst[i][1]]);
    }
  }

  private computeDisplaySpectrum(phaseNoise: Float32Array, nstart: number): void {
    this.negative_plot_data = [];
    for (let i = 0; i < this.n_pts; i++) {
      const f = this.plot_data[i][0];
      const value = phaseNoise[i] * (this.laserPlotType === 'phase' ? 0.5 : f ** 2);
      this.linear_plot_data[i][1] = i >= nstart - 1 ? value : NaN;
      this.plot_data[i][1] = i >= nstart - 1 && Number.isFinite(value) && value !== 0
        ? 10 * Math.log10(Math.abs(value)) : NaN;
      if (i >= nstart - 1 && value < 0) this.negative_plot_data.push([f, this.plot_data[i][1]]);
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
      const ddsFreq = await app.dds.getDDSFreq(this.driver.parameters.channel === 1 ? 2 : 0);

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
      this.signed_phase_psd = phaseNoise;

      if (this.n_pts !== phaseNoise.length) {
        this.n_pts = phaseNoise.length;
        this.setFreqAxis();
      }

      this.ensurePlotBuffer();

      const nstart = 3;
      if (this.displayedChannel !== this.driver.parameters.channel) {
        this.displayedChannel = this.driver.parameters.channel;
        this.plotBasics.setLinY();
      }
      this.yLabel = this.laserPlotType === 'phase'
        ? "PHASE NOISE MAGNITUDE (dBc/Hz)" : "FREQUENCY NOISE MAGNITUDE (dB Hz²/Hz)";
      this.computeDisplaySpectrum(phaseNoise, nstart);

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
        false,
        this.displayedChannel === 2 ? [{ label: 'Negative estimate', data: this.negative_plot_data, color: '#c43b36',
           lines: { show: false }, points: { show: true, radius: 1.5, lineWidth: 1 } },
         ...(smoothedVisible ? [{ label: 'Negative smoothed estimate', data: this.negative_smooth_data,
           color: '#c43b36', lines: { show: false }, points: { show: true, radius: 3, lineWidth: 1 } }] : [])] : [],
        `${this.yLabel} (smoothed)`
      );
    } catch (err) {
      console.error('updatePlot error:', err);
      this._busy = false;
      setTimeout(() => requestAnimationFrame(() => this.updatePlot()), 500);
    }
  }
}
