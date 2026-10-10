// Plot widget
// (c) Koheron

interface PnaPlotParameters {
  data_size: number;
  fs: number;
  channel: number;
  cic_rate: number;
  fft_navg: number;
}

interface PnaPlotDriver<P> {
  parameters: P;
  getPhaseNoise(): Promise<Float32Array>;
  getSpectrumSnapshot?(): Promise<PnaSpectrumFrame<P>>;
}

// Acquisition topology and signed-density behavior are supplied by the board.
abstract class PnaPlot<P extends PnaPlotParameters> {
  protected abstract signedSpectrum(): boolean;
  protected abstract localOscillators(parameters: P): number[];
  protected abstract referenceLabel(parameters: P): string;

  private validDensity(value: number): boolean {
    return Number.isFinite(value) && (this.signedSpectrum() ? value !== 0 : value > 0);
  }
  private disposed = false;
  public n_pts: number;
  public plot: jquery.flot.plot;
  public plot_data: Array<Array<number>>;
  private negative_plot_data: number[][] = [];
  private negative_smooth_data: number[][] = [];
  public phase_psd: Float32Array = new Float32Array(0);
  public smooth_plot_data: Array<Array<number>>;
  private linear_plot_data: Array<Array<number>>;
  private samplingFrequency: number;
  private decadeValuesTable: HTMLTableElement;
  private frameParameters: P;
  private lastReplyParameters: P;
  private frameSequence: number;
  private renderedPlotType: 'phase' | 'frequency';
  public frameReceivedAt: string;
  public references: PnaReferences<P>;
  private referencePanel: PlotReferencePanel<PnaReference<P>>;
  private hasInitialFit = false;
  public get visibleReferences(): PnaReference<P>[] {
    this.updateReferenceDisplay();
    return this.references.items.filter(item => item.visible);
  }
  public get frameStatus(): P { return this.frameParameters; }

  private laserPlotTypeInputs: HTMLInputElement[];
  private showSmoothedInput: HTMLInputElement;
  private laserPlotType: 'phase' | 'frequency' = 'phase';

  public yLabel: string = 'Phase noise magnitude (dBc/Hz)';
  private peakDatapoint: number[];

  constructor(private document: Document, private driver: PnaPlotDriver<P>, public plotBasics: PlotBasics,
      private onConnectionError: (error: unknown) => void = () => {}) {
    this.peakDatapoint = [];
    this.plot_data = [];
    this.smooth_plot_data = [];
    this.linear_plot_data = [];
    this.initReferences();
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
    this.plotBasics.enableBatchedLines();
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
      const magnitude = this.signedSpectrum() ? ' magnitude' : '';
      this.yLabel = this.laserPlotType === 'phase' ? `Phase noise${magnitude} (dBc/Hz)` : `Frequency noise${magnitude} (dB Hz²/Hz)`;
      const unit = document.getElementById('spectrum-unit');
      if (unit) { unit.textContent = this.laserPlotType === 'phase' ? 'dBc/Hz' : 'dB Hz²/Hz'; }
      if (this.frameParameters) {
        this.computeDisplaySpectrum(this.phase_psd, 2);
        this.computeSmoothedPlot(2);
        this.renderedPlotType = this.laserPlotType;
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
  private _targetHz = 60; // Cached replies do not count as newly displayed spectra.
  private _lastTick = -Infinity;
  private lastStarted = -Infinity;
  private timer: number;
  private animation: number;
  private rateStarted = performance.now();
  private displayedFrames = 0;
  private receivedFrames = 0;
  private readMs = 0;
  private processMs = 0;
  private drawMs = 0;
  private schedulerMs = 0;
  private lastTableUpdate = -Infinity;
  private visibilityHandler = () => {
    window.clearTimeout(this.timer);
    window.cancelAnimationFrame(this.animation);
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
      data = this.plot_data, linear = this.linear_plot_data, negative = this.negative_plot_data || (this.negative_plot_data = [])): void {
    negative.length = 0;
    for (let i = 0; i < phaseNoise.length; i++) {
      const f = data[i][0];
      const value = phaseNoise[i] * (this.laserPlotType === 'phase' ? 0.5 : f * f);
      linear[i][1] = i >= firstBin && Number.isFinite(value) && (this.signedSpectrum() || value >= 0) ? value : NaN;
      data[i][1] = i >= firstBin && this.validDensity(value)
        ? 10 * Math.log10(Math.abs(value)) : NaN;
      if (this.signedSpectrum() && i >= firstBin && value < 0) { negative.push([f, data[i][1]]); }
    }
  }

  private computeSmoothedPlot(firstBin: number, src = this.linear_plot_data, dst = this.smooth_plot_data, negative = this.negative_smooth_data || (this.negative_smooth_data = [])): void {
    negative.length = 0;
    // Display smoothing: average linear density in a 0.1-decade
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
      dst[i][1] = this.validDensity(mean) ? 10 * Math.log10(Math.abs(mean)) : NaN;
      if (this.signedSpectrum() && mean < 0) { negative.push([f, dst[i][1]]); }
    }
  }

  private initReferences(): void {
    this.references = new PnaReferences<P>(this.document.body.dataset.board || 'analyzer', this.signedSpectrum());
    this.referencePanel = new PlotReferencePanel(this.document, this.references, () => {
      this.updateReferenceDisplay();
      this.updateReferenceControls();
      this.plotBasics.refreshLegend();
      this.redraw(() => {});
    }, item => this.replaceReference(item), item => {
      const text = this.referenceLabel(item.parameters);
      return {text, title:text + ' · ' + item.parameters.fs + ' samples/s · ' + item.psd.length + ' bins'};
    });
  }

  captureReference(): void {
    if (!this.frameParameters || this.references.items.length >= PlotReferences.limit ||
        !this.phase_psd.subarray(2).some(v => this.validDensity(v))) { return; }
    this.references.capture(this.phase_psd, this.frameParameters, this.frameReceivedAt);
  }

  replaceReference(item: PnaReference<P>): void {
    if (!this.frameParameters || !this.phase_psd.subarray(2).some(v => this.validDensity(v))) { return; }
    this.references.replace(item, this.phase_psd, this.frameParameters, this.frameReceivedAt);
  }

  clearReference(): void { this.references.clear(); }

  private updateReferenceDisplay(): void {
    for (const item of this.references.items) {
      if (item.data && item.plotType === this.laserPlotType) { continue; }
      const {psd, parameters} = item;
      const binWidth = parameters.fs / (2 * (psd.length - 1));
      item.data = Array.from(psd, (_, i) => [i * binWidth, NaN]);
      item.smooth = item.data.map(row => row.slice());
      const linear = item.data.map(row => row.slice());
      item.negative = []; item.negativeSmooth = [];
      this.computeDisplaySpectrum(psd, 2, item.data, linear, item.negative);
      this.computeSmoothedPlot(2, linear, item.smooth, item.negativeSmooth);
      item.plotType = this.laserPlotType;
    }
  }

  private updateReferenceControls(): void {
    const ready = !!this.frameParameters;
    const full = this.references.items.length >= PlotReferences.limit;
    const capture = this.document.getElementById('capture-reference') as HTMLButtonElement;
    if (capture) {
      capture.disabled = !ready || full;
      capture.title = full ? '8 references captured. Recapture or remove an existing reference.'
        : ready ? 'Capture the displayed live spectrum' : 'Waiting for a valid live spectrum';
    }
    (this.document.getElementById('clear-reference') as HTMLButtonElement).disabled = !this.references.items.length;
    this.document.querySelectorAll<HTMLButtonElement>('.replace-reference').forEach(button => { button.disabled = !ready; });
  }

  private captureReady: boolean;
  private setCaptureReady(ready: boolean): void {
    if (!ready) { this.frameParameters = undefined; }
    // Readiness rarely changes. Rewriting titles, disabled states and ARIA on
    // every frame also wakes browser/extension observers at the display rate.
    if (this.captureReady === ready) { return; }
    this.captureReady = ready;
    if (ready) {
      const status = this.document.getElementById('spectrum-status');
      if (status) { status.hidden = true; }
    }
    this.updateReferenceControls();
    const fit = document.getElementById('fit-view') as HTMLButtonElement;
    if (fit) { fit.disabled = !ready; }
    document.querySelectorAll<HTMLButtonElement>('.export-data, .export-plot').forEach(button => {
      button.disabled = !ready;
      button.title = ready ? (button.classList.contains('export-data') ? 'Export the displayed spectrum as CSV' : 'Export the noise plot as PNG')
        : 'Waiting for a valid live spectrum';
    });
    this.document.getElementById('plot-placeholder')?.setAttribute('aria-label',
      ready ? 'Live noise spectrum' : 'Noise spectrum; no live data');
  }

  public markUnavailable(reason: string, resetRate = true): void {
    this.setCaptureReady(false);
    if (resetRate) { this.resetRate(); }
    this.document.getElementById('plot-placeholder')?.setAttribute('aria-label',
      `${reason}; spectrum is not live`);
    const plot = this.document.getElementById('plot-placeholder');
    if (plot) {
      let status = this.document.getElementById('spectrum-status');
      if (!status) {
        status = this.document.createElement('p');
        status.id = 'spectrum-status';
        status.setAttribute('role', 'status');
        plot.parentElement.insertBefore(status, plot);
      }
      status.textContent = reason;
      status.hidden = false;
    }
  }

  private redraw(callback: () => void): void {
    if (!this.plot_data.length) { return; }
    const references = this.visibleReferences;
    this.plotBasics.redraw(this.plot_data, this.n_pts, this.peakDatapoint, this.yLabel, callback,
      undefined, false,
      [
        ...(this.showSmoothedInput?.checked ? [{label: 'Smoothed', data: this.smooth_plot_data, color: '#006400'}] : []),
        ...references.map(item => ({label:PlotReferences.traceLabel(item.name), color:item.color,
          data:this.showSmoothedInput?.checked ? item.smooth : item.data})),
        ...(this.signedSpectrum() ? [{label: 'Negative estimates', data: this.negative_plot_data, color: '#c33', lines: {show: false}, points: {show: true, radius: 1.5}},
          {data: this.showSmoothedInput?.checked ? this.negative_smooth_data : [], color: '#c33', lines: {show: false}, points: {show: true, radius: 1.5}},
          ...references.map(item => ({data:this.showSmoothedInput?.checked ? item.negativeSmooth : item.negative,
            color:item.color, lines:{show:false}, points:{show:true, radius:1.5}}))] : [])
      ]);
  }

  private spectrumChanged(next: Float32Array): boolean {
    if (!this.phase_psd || next.length !== this.phase_psd.length) { return true; }
    for (let i = 0; i < next.length; i++) {
      // Identical nonfinite bins are not a new frame.
      if (!Object.is(next[i], this.phase_psd[i])) { return true; }
    }
    return false;
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
    // Allow sub-millisecond clock jitter around a display refresh boundary.
    if (sinceLast + 1 < frameBudgetMs) {
      this._busy = false;
      this.schedule(frameBudgetMs - sinceLast);
      return;
    }

    // Keep the intended cadence instead of adding each callback's lateness to
    // the next deadline. After a long pause, start a new cadence without a burst.
    this._lastTick = Number.isFinite(this._lastTick) && sinceLast < Math.max(250, 2 * frameBudgetMs)
      ? this._lastTick + frameBudgetMs : now;
    const frameDelay = Number.isFinite(this.lastStarted) ? Math.max(0, now - this.lastStarted - frameBudgetMs) : 0;
    this.lastStarted = now;

    try {
      // The controls already refresh these parameters and read back LO edits.
      // Avoid an extra RPC round trip for every displayed spectrum.
      const parameters = this.driver.parameters;
      const frequencies = this.localOscillators(parameters);

      if (!frequencies.every(frequency => this.loIsSet(frequency))) {
        this.markUnavailable('Local oscillator not set');
        this._busy = false;
        this.schedule(this._loBackoffMs);
        return;
      }

      const readStarted = performance.now();
      const snapshot = this.driver.getSpectrumSnapshot
        ? await this.driver.getSpectrumSnapshot()
        : {sequence: undefined, state: undefined, parameters: {...this.driver.parameters}, values: await this.driver.getPhaseNoise()};
      const frameParameters = snapshot.parameters;
      const phaseNoise = snapshot.values;
      const received = performance.now();
      if (this.disposed) { return; }
      if (this.document.hidden) { this._busy = false; return; }
      if (phaseNoise.length < 3) {
        this.markUnavailable('Measurement unavailable');
        this._busy = false;
        this.schedule(500);
        return;
      }
      const changed = snapshot.sequence === undefined ? this.spectrumChanged(phaseNoise)
        : snapshot.sequence !== this.frameSequence;
      const ready = (snapshot.state === undefined || snapshot.state === 1) && phaseNoise.subarray(2).some(v => this.validDensity(v));
      if (!changed && this.lastReplyParameters && this.captureReady === ready && this.renderedPlotType === this.laserPlotType &&
          (Object.keys(frameParameters) as (keyof P)[]).every(key => Object.is(frameParameters[key], this.lastReplyParameters[key]))) {
        // The server returns its last published PSD between acquisitions.
        // A zoom/resize still needs a redraw, but keeps the capture timestamp.
        const drawStarted = performance.now();
        const complete = () => {
          this._busy = false;
          if (this.disposed) { return; }
          this.recordFrame(received - readStarted, 0, performance.now() - drawStarted, frameDelay, false);
          this.schedule(Math.max(0, this._lastTick + frameBudgetMs - performance.now()));
        };
        if (this.plotBasics.needsRedraw()) { this.redraw(complete); }
        else { complete(); }
        return;
      }
      this.phase_psd = phaseNoise;
      this.frameSequence = snapshot.sequence;
      if (this.n_pts !== phaseNoise.length || this.samplingFrequency !== frameParameters.fs) {
        this.samplingFrequency = frameParameters.fs;
        this.n_pts = phaseNoise.length;
        this.setFreqAxis();
      }
      this.ensurePlotBuffer();

      this.computeDisplaySpectrum(phaseNoise, 2);
      this.computeSmoothedPlot(2);
      this.renderedPlotType = this.laserPlotType;
      this.frameParameters = {...frameParameters, fs: this.samplingFrequency, data_size: phaseNoise.length};
      this.lastReplyParameters = {...frameParameters};
      this.frameReceivedAt = new Date().toISOString();
      this.setCaptureReady(ready);
      if (ready) {
        if (!this.hasInitialFit) {
          this.plotBasics.setLinY();
          this.hasInitialFit = true;
        }
      } else {
        const live = snapshot.state === 1;
        const allZero = phaseNoise.subarray(2).every(v => v === 0);
        // Connected replies contribute polling timings even while acquisition settles.
        this.markUnavailable(!live ? 'Acquisition settling' : allZero
          ? 'No noise resolved at this precision. Increase phase precision to resolve smaller changes.'
          : 'No valid noise spectrum available', false);
      }
      this.updateReferenceDisplay();

      // Numeric readouts need a lower cadence than the spectrum animation.
      if (now - this.lastTableUpdate >= 250) {
        this.setDecadeValuesTable();
        this.lastTableUpdate = now;
      }

      const drawStarted = performance.now();
      this.redraw(() => {
          this._busy = false;
          if (this.disposed) { return; }
          const drawn = performance.now();
          this.recordFrame(received - readStarted, drawStarted - received, drawn - drawStarted,
            frameDelay, ready && changed);
          this.schedule(Math.max(0, this._lastTick + frameBudgetMs - drawn));
        });
    } catch (err) {
      if (this.disposed) { return; }
      console.error('updatePlot error:', err);
      this.markUnavailable('Measurement unavailable');
      this.onConnectionError?.(err);
      this._busy = false;
      this.schedule(500);
    }
  }

  private schedule(delay: number): void {
    window.clearTimeout(this.timer);
    window.cancelAnimationFrame(this.animation);
    if (this.disposed || this.document.hidden) { return; }
    const update = () => {
      window.clearTimeout(this.timer);
      window.cancelAnimationFrame(this.animation);
      void this.updatePlot();
    };
    // Some visible/occluded browser windows throttle screen callbacks to 1 Hz.
    // Race a deadline timer against the screen callback, as in the FFT display;
    // whichever wins cancels the other, keeping only one serial read loop.
    this.timer = window.setTimeout(update, Math.ceil(delay));
    if (delay <= 1000 / this._targetHz) {
      this.animation = window.requestAnimationFrame(() => {
        if (performance.now() + 1 >= this._lastTick + 1000 / this._targetHz) { update(); }
      });
    }
  }

  private resetRate(): void {
    this.rateStarted = performance.now();
    this.displayedFrames = 0;
    this.receivedFrames = 0;
    this.lastStarted = -Infinity;
    this.readMs = this.processMs = this.drawMs = this.schedulerMs = 0;
    const rate = this.document.getElementById('refresh-rate');
    if (rate) {
      rate.textContent = '— FPS';
      rate.title = `New spectra displayed per second; polling target ${this._targetHz}/s; cached replies excluded`;
    }
  }

  private recordFrame(readMs = 0, processMs = 0, drawMs = 0, schedulerMs = 0, changed = true): void {
    if (this.document.hidden) { return; }
    if (changed) { this.displayedFrames++; }
    this.receivedFrames++;
    this.readMs += readMs;
    this.processMs += processMs;
    this.drawMs += drawMs;
    this.schedulerMs += schedulerMs;
    const elapsed = performance.now() - this.rateStarted;
    if (elapsed < 1000) { return; }
    const rate = this.document.getElementById('refresh-rate');
    if (rate) {
      rate.textContent = (this.displayedFrames * 1000 / elapsed).toFixed(0) + ' FPS';
      const mean = (time: number) => (time / this.receivedFrames).toFixed(1);
      rate.title = `New spectra displayed per second; polling ${(this.receivedFrames * 1000 / elapsed).toFixed(0)}/s (target ${this._targetHz}/s)`
        + `; read ${mean(this.readMs)} ms; process ${mean(this.processMs)} ms`
        + `; draw ${mean(this.drawMs)} ms; scheduling delay ${mean(this.schedulerMs)} ms`;
    }
    this.rateStarted = performance.now();
    this.displayedFrames = 0;
    this.receivedFrames = 0;
    this.readMs = this.processMs = this.drawMs = this.schedulerMs = 0;
  }

  dispose(): void {
    this.disposed = true;
    this.referencePanel?.dispose();
    window.clearTimeout(this.timer);
    window.cancelAnimationFrame(this.animation);
    this.document.removeEventListener('visibilitychange', this.visibilityHandler);
    this.resetRate();
  }
}
