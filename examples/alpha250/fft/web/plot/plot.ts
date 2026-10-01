// (c) Koheron

// Spectrum display. Hardware acquisition continues when the display is paused.
class Plot {
    private running = true;
    private paused = false;
    private busy = false;
    private timer: number;
    private animation = 0;
    private drawTimer: number;
    private stream: PSDStream;
    private lastFrameTime = -Infinity;
    private pending: {psd: Float32Array; status: IFFTStatus};
    private rateStarted = performance.now();
    private renderedFrames = 0;
    private acquiredFrames = 0;
    private renderMs = 0;
    private historyMs = 0;
    private waitMs = 0;
    private visibilityHandler = () => {
        window.clearTimeout(this.timer);
        window.cancelAnimationFrame(this.animation);
        window.clearTimeout(this.drawTimer);
        this.animation = 0;
        this.pending = undefined;
        this.resetRate();
        if (this.stream) { this.stream.setActive(!this.document.hidden && !this.paused); }
        else if (!this.document.hidden) { this.updatePlot(); }
    };
    private samplingFrequency = 0;
    private peak: number[] = [];
    private psd: Float32Array;
    private reference: {psd: Float32Array; status: IFFTStatus};
    private referenceUnit: string;
    public reference_data: number[][];
    public get referenceStatus(): IFFTStatus { return this.reference && this.reference.status; }
    public history = new SpectrumHistory();
    private views: SpectrumViews;
    public average_data: number[][];
    public maximum_data: number[][];
    public get view(): string { return this.views ? this.views.mode : 'spectrum'; }
    public n_pts: number;
    public plot_data: number[][] = [];
    public yLabel = 'PSD (dBm/Hz)';
    public unit = 'dBm-Hz';
    public frameStatus: IFFTStatus;

    constructor(private document: Document, private fft: FFT, private plotBasics: PlotBasics) {
        this.n_pts = fft.fft_size / 2;
        // Reduce only the drawn curves; measurements and exports retain every bin.
        this.plotBasics.enableSpectrumReduction();
        this.plotBasics.setLinY();
        for (const input of Array.from(document.querySelectorAll<HTMLInputElement>('.unit-input'))) {
            input.addEventListener('change', () => {
                this.plotBasics.setLinY();
                if (this.psd) { this.displaySpectrum(); }
            });
        }
        const canvas = document.getElementById('history-canvas') as HTMLCanvasElement;
        if (canvas && typeof canvas.getContext === 'function') {
            this.views = new SpectrumViews(document, this.history, () => this.plotBasics.getRangeX(),
                (power, unit) => this.convertValue(power, unit, this.history.status),
                (from, to) => { this.plotBasics.setVisibleRangeX(from, to); this.redraw(); },
                () => { this.plotBasics.setLinY(); if (this.psd) { this.displaySpectrum(); } });
        }
        for (const id of ['average-trace', 'max-hold-trace']) {
            document.getElementById(id).addEventListener('change', () => {
                this.plotBasics.setLinY(); if (this.psd) { this.displaySpectrum(); }
            });
        }
        document.getElementById('clear-history').addEventListener('click', () => {
            this.history.reset(); this.average_data = this.maximum_data = undefined;
            this.plotBasics.setLinY(); this.redraw();
        });
        document.getElementById('exclude-dc').addEventListener('change', () => this.redraw());
        document.getElementById('capture-reference').addEventListener('click', () => this.captureReference());
        document.getElementById('clear-reference').addEventListener('click', () => this.clearReference());
        document.querySelector('.peak-input').addEventListener('change', () => {
            if (this.plot_data.length) { this.redraw(); }
        });
        $('#plot-placeholder').on('plotselected.fft dblclick.fft wheel.fft', () => {
            if (this.plot_data.length) { this.redraw(); }
        });
        document.addEventListener('visibilitychange', this.visibilityHandler);
        if (typeof Worker !== 'undefined' && typeof fft.startPSDStream === 'function') {
            try {
                this.stream = fft.startPSDStream((psd, time) => {
                    if (!this.running || this.paused || this.document.hidden) { return; }
                    this.acceptSpectrum(psd, time); this.updateRate();
                }, message => {
                    if (this.running && !this.paused && !this.document.hidden) {
                        this.pending = undefined;
                        window.cancelAnimationFrame(this.animation);
                        window.clearTimeout(this.drawTimer);
                        this.animation = 0;
                        this.setStatus('error', message);
                    }
                });
                if (document.hidden) { this.stream.setActive(false); }
            } catch (error) {
                console.warn('Using main-thread spectrum polling:', error);
                this.updatePlot();
            }
        } else { this.updatePlot(); }
    }

    setPaused(paused: boolean): void {
        this.paused = paused;
        if (paused) {
            window.clearTimeout(this.timer);
            window.cancelAnimationFrame(this.animation);
            window.clearTimeout(this.drawTimer);
            this.animation = 0;
            this.pending = undefined;
        }
        this.resetRate();
        this.setStatus(paused ? 'paused' : 'connecting', paused ? 'Display paused' : 'Resuming…');
        if (this.stream) { this.stream.setActive(!paused && !this.document.hidden); }
        else if (!paused) { this.updatePlot(); }
    }

    private setStatus(state: string, text: string): void {
        const status = this.document.getElementById('connection-status');
        status.dataset.state = state;
        status.textContent = text;
    }

    private schedule(delay: number): void {
        window.clearTimeout(this.timer);
        if (!this.running || this.paused || this.document.hidden) { return; }
        this.timer = window.setTimeout(() => this.updatePlot(), delay);
    }

    private requestDraw(): void {
        if (this.animation || !this.running || this.paused || this.document.hidden) { return; }
        const requested = performance.now();
        const draw = (timestamp: number) => {
            window.cancelAnimationFrame(this.animation);
            window.clearTimeout(this.drawTimer);
            this.animation = 0;
            this.waitMs += performance.now() - requested;
            if (!this.running || this.paused || this.document.hidden || !this.pending) { return; }
            if (timestamp - this.lastFrameTime < 1000 / 60 - .5) {
                this.requestDraw();
                return;
            }
            this.lastFrameTime = timestamp;
            this.psd = this.pending.psd;
            this.frameStatus = this.pending.status;
            this.pending = undefined;
            try {
                const started = performance.now();
                this.displaySpectrum();
                this.renderMs += performance.now() - started;
                this.renderedFrames++;
                this.setStatus('live', 'Live spectrum');
            } catch (error) {
                this.setStatus('error', 'Unable to display spectrum');
                console.error('Spectrum display failed:', error);
            }
        };
        this.animation = window.requestAnimationFrame(draw);
        // Some visible browser windows delay animation callbacks despite fast
        // acquisition. Keep the latest frame responsive, without a second loop
        // or an accumulating queue. Whichever callback wins cancels the other.
        const delay = Math.max(1, Math.ceil(1000 / 60 - (requested - this.lastFrameTime)));
        this.drawTimer = window.setTimeout(() => draw(performance.now()), delay);
    }

    private resetRate(): void {
        this.rateStarted = performance.now();
        this.renderedFrames = this.acquiredFrames = 0;
        this.renderMs = this.historyMs = this.waitMs = 0;
        const rate = this.document.getElementById('refresh-rate');
        if (rate) {
            rate.textContent = this.paused ? 'Paused' : '— FPS';
            rate.title = 'Fresh spectra displayed per second';
        }
    }

    private updateRate(): void {
        const elapsed = performance.now() - this.rateStarted;
        if (elapsed < 1000) { return; }
        const rate = this.document.getElementById('refresh-rate');
        if (rate) {
            rate.textContent = (this.renderedFrames * 1000 / elapsed).toFixed(0) + ' FPS';
            rate.title = 'Fresh spectra displayed per second; acquisition: '
                + (this.acquiredFrames * 1000 / elapsed).toFixed(0) + ' spectra/s'
                + '; render ' + (this.renderMs / Math.max(1, this.renderedFrames)).toFixed(2) + ' ms'
                + '; history ' + (this.historyMs / Math.max(1, this.acquiredFrames)).toFixed(2) + ' ms'
                + '; frame wait ' + (this.waitMs / Math.max(1, this.renderedFrames)).toFixed(1) + ' ms';
        }
        this.rateStarted = performance.now();
        this.renderedFrames = this.acquiredFrames = 0;
        this.renderMs = this.historyMs = this.waitMs = 0;
    }

    async updatePlot(): Promise<void> {
        if (!this.running || this.paused || this.document.hidden || this.busy) { return; }
        this.busy = true;
        const started = performance.now();
        let delay = 1000 / 60;
        try {
            const psd = await this.fft.read_psd();
            if (!this.running || this.paused || this.document.hidden) { return; }
            this.acceptSpectrum(psd, performance.now() / 1000);
        } catch (error) {
            if (!this.running || this.paused || this.document.hidden) { return; }
            this.pending = undefined;
            window.cancelAnimationFrame(this.animation);
            window.clearTimeout(this.drawTimer);
            this.animation = 0;
            this.setStatus('error', 'Waiting for spectrum…');
            console.error('Spectrum update failed:', error);
            delay = 1000;
        } finally {
            this.busy = false;
            this.updateRate();
            this.schedule(delay === 1000 ? delay : Math.max(0, delay - (performance.now() - started)));
        }
    }

    private acceptSpectrum(psd: Float32Array, time: number): void {
        if (!psd.some(value => Number.isFinite(value) && value > 0)) {
            this.setStatus('connecting', 'Waiting for spectrum…'); return;
        }
        this.pending = {psd: psd.slice(), status: {...this.fft.status, dds_freq: this.fft.status.dds_freq.slice()}};
        const historyStarted = performance.now();
        if (this.history) { this.history.add(psd, this.pending.status, time); }
        this.historyMs += performance.now() - historyStarted;
        this.acquiredFrames++;
        this.requestDraw();
    }

    private displaySpectrum(): void {
        this.unit = this.document.querySelector<HTMLInputElement>('.unit-input:checked').value;
        this.yLabel = this.unit === 'dBm-Hz' ? 'PSD (dBm/Hz)' : this.unit === 'dBm' ? 'Power (dBm)' : 'Voltage noise (nV/√Hz)';
        const fs = this.frameStatus.fs;
        if (fs !== this.samplingFrequency) {
            this.samplingFrequency = fs;
            this.plotBasics.setRangeX(0, fs / 2e6);
        }
        // Bin k is at k * fs / FFT size. The server returns N/2 bins,
        // including DC and excluding Nyquist; never add a synthetic tail bin.
        const length = Math.min(this.n_pts, this.psd.length);
        this.plot_data.length = length;
        for (let i = 0; i < length; i++) {
            const row = this.plot_data[i] || (this.plot_data[i] = [0, 0]);
            row[0] = i * fs / this.fft.fft_size / 1e6;
            row[1] = this.convertValue(this.psd[i], this.unit);
        }
        this.document.getElementById('bin-spacing').textContent = (fs / this.fft.fft_size / 1000).toFixed(3) + ' kHz';
        this.document.getElementById('fft-size').textContent = this.fft.fft_size.toLocaleString() + ' points';
        // A captured frame is immutable: convert it only after capture or a unit change.
        if (this.reference && (!this.reference_data || this.referenceUnit !== this.unit)) {
            this.reference_data = Array.from(this.reference.psd, (value, index) => [
                index * this.reference.status.fs / this.fft.fft_size / 1e6,
                this.convertValue(value, this.unit, this.reference.status)
            ]);
            this.referenceUnit = this.unit;
        }
        const convertTrace = (values: Float32Array, data: number[][]): number[][] => {
            if (!values) { return undefined; }
            data = data || []; data.length = values.length;
            for (let i = 0; i < values.length; i++) {
                const row = data[i] || (data[i] = [0, 0]);
                row[0] = i * this.history.status.fs / this.fft.fft_size / 1e6;
                row[1] = this.convertValue(values[i], this.unit, this.history.status);
            }
            return data;
        };
        this.average_data = (this.document.getElementById('average-trace') as HTMLInputElement).checked && this.view === 'spectrum' ? convertTrace(this.history.average, this.average_data) : undefined;
        this.maximum_data = (this.document.getElementById('max-hold-trace') as HTMLInputElement).checked && this.view === 'spectrum' ? convertTrace(this.history.maximum, this.maximum_data) : undefined;
        this.document.getElementById('reference-info').hidden = !this.reference || this.view !== 'spectrum';
        this.redraw();

    }

    captureReference(): void {
        if (!this.psd || !this.frameStatus) { return; }
        this.reference = {psd: this.psd.slice(0, this.plot_data.length), status: {...this.frameStatus, dds_freq: this.frameStatus.dds_freq.slice()}};
        this.reference_data = undefined;
        this.document.getElementById('capture-reference').textContent = 'Replace ref';
        (this.document.getElementById('clear-reference') as HTMLButtonElement).disabled = false;
        this.document.getElementById('reference-info').hidden = false;
        const windows = ['Rectangular', 'Hann', 'Flat top', 'Blackman–Harris'];
        this.document.getElementById('reference-status').textContent = 'ADC ' + this.reference.status.channel
            + ' · ' + (windows[this.reference.status.window_index] || 'Window ' + this.reference.status.window_index) + ' · ' + this.reference.status.fs / 1e6 + ' MS/s';
        this.plotBasics.setLinY();
        this.displaySpectrum();
    }

    clearReference(): void {
        this.reference = undefined;
        this.reference_data = undefined;
        this.document.getElementById('capture-reference').textContent = 'Capture ref';
        (this.document.getElementById('clear-reference') as HTMLButtonElement).disabled = true;
        this.document.getElementById('reference-info').hidden = true;
        this.plotBasics.setLinY();
        this.redraw();
    }

    private redraw(): void {
        const ready = this.plot_data.length > 0;
        (this.document.getElementById('capture-reference') as HTMLButtonElement).disabled = !ready || this.view !== 'spectrum';
        for (const button of Array.from(this.document.querySelectorAll<HTMLButtonElement>('.export-data, .export-plot'))) {
            button.disabled = !ready || (this.view !== 'spectrum' && !this.history.samples);
        }
        if (!this.plot_data.length) { return; }
        const range = this.plotBasics.getRangeX();
        const excludeDC = (this.document.getElementById('exclude-dc') as HTMLInputElement).checked;
        let peak: number[] = [];
        for (const row of this.plot_data) {
            if (row[0] < range.from || row[0] > range.to || (excludeDC && row[0] === 0)) { continue; }
            if (Number.isFinite(row[1]) && (!peak.length || row[1] > peak[1])) { peak = row.slice(); }
        }
        this.document.getElementById('peak-frequency').textContent = peak.length ? peak[0].toFixed(6) + ' MHz' : '—';
        const unitLabel = this.unit === 'dBm-Hz' ? 'dBm/Hz' : this.unit === 'dBm' ? 'dBm' : 'nV/√Hz';
        this.document.getElementById('peak-level').textContent = peak.length ? peak[1].toFixed(2) + ' ' + unitLabel : '—';
        this.peak = peak;
        if (this.view !== 'spectrum') { this.views.render(this.unit, this.yLabel, this.paused); return; }
        const traces: {label: string; color: string; data: number[][]}[] = [];
        if (this.average_data) { traces.push({label: 'Average', color: '#389168', data: this.average_data}); }
        if (this.maximum_data) { traces.push({label: 'Max hold', color: '#ba861a', data: this.maximum_data}); }
        this.plotBasics.redraw(this.plot_data, this.plot_data.length, this.peak.slice(), this.yLabel, () => {}, this.reference_data, true, traces);
    }

    convertValue(value: number, unit: string, status: IFFTStatus = this.frameStatus || this.fft.status): number {
        if (!Number.isFinite(value) || value < 0) { return NaN; }
        if (unit === 'dBm-Hz') { return 10 * Math.log10(value / 1e-3); }
        if (unit === 'dBm') {
            return 10 * Math.log10(value * (status.W2 / status.W1) * status.fs / this.fft.fft_size / 1e-3);
        }
        return Math.sqrt(50 * value) * 1e9;
    }

    dispose(): void {
        this.running = false;
        if (this.stream) { this.stream.dispose(); }
        if (this.views) { this.views.dispose(); }
        $('#plot-placeholder').off('.fft');
        window.clearTimeout(this.timer);
        window.cancelAnimationFrame(this.animation);
        window.clearTimeout(this.drawTimer);
        this.document.removeEventListener('visibilitychange', this.visibilityHandler);
        this.pending = undefined;
    }
}
