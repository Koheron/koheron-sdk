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
    private paintedFrames = 0;
    private paintFailed = false;
    private acquiredFrames = 0;
    private renderMs = 0;
    private historyMs = 0;
    private waitMs = 0;
    private resizeHandler = () => {
        if (this.running && !this.document.hidden && this.psd && this.plotBasics.needsRedraw()) {
            // A paused spectrum still needs fresh column reduction after resize.
            this.redraw();
        }
    };
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

    constructor(private document: Document, private fft: FFTDriver, private plotBasics: PlotBasics) {
        this.n_pts = fft.status.spectrum ? fft.status.spectrum.frequencies.length : fft.fft_size / 2;
        // Reduce only the drawn curves; measurements and exports retain every bin.
        this.plotBasics.enableSpectrumReduction();
        this.plotBasics.enableBatchedLines();
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
                (power, unit, index) => this.convertValue(power, unit, this.history.status, index),
                (from, to) => { this.plotBasics.setVisibleRangeX(from, to); this.redraw(); },
                () => { this.plotBasics.setLinY(); if (this.psd) { this.displaySpectrum(); } },
                fft.status.spectrum);
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
        window.addEventListener('resize', this.resizeHandler);
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
        if (status.dataset.state !== state) { status.dataset.state = state; }
        if (status.textContent !== text) { status.textContent = text; }
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
            if (!this.running || this.paused || this.document.hidden || !this.pending) { return; }
            this.waitMs += performance.now() - requested;
            if (timestamp - this.lastFrameTime < 1000 / 60 - .5) {
                this.requestDraw();
                return;
            }
            this.lastFrameTime = timestamp;
            const fresh = this.spectrumChanged(this.pending.psd);
            const previousPSD = this.psd, previousStatus = this.frameStatus;
            this.psd = this.pending.psd;
            this.frameStatus = this.pending.status;
            this.pending = undefined;
            try {
                const started = performance.now();
                this.displaySpectrum();
                this.paintFailed = false;
                this.renderMs += performance.now() - started;
                this.paintedFrames++;
                if (fresh) { this.renderedFrames++; }
                this.setStatus('live', 'Live spectrum');
            } catch (error) {
                this.psd = previousPSD;
                this.frameStatus = previousStatus;
                this.paintFailed = true;
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
        this.renderedFrames = this.paintedFrames = this.acquiredFrames = 0;
        this.renderMs = this.historyMs = this.waitMs = 0;
        const rate = this.document.getElementById('refresh-rate');
        if (rate) {
            rate.textContent = this.paused ? 'Paused' : '— FPS';
            rate.title = 'Changed spectra displayed per second; cached replies excluded';
        }
    }

    private updateRate(): void {
        const elapsed = performance.now() - this.rateStarted;
        if (elapsed < 1000) { return; }
        const rate = this.document.getElementById('refresh-rate');
        if (rate) {
            rate.textContent = (this.renderedFrames * 1000 / elapsed).toFixed(0) + ' FPS';
            rate.title = 'Changed spectra displayed per second; received: '
                + (this.acquiredFrames * 1000 / elapsed).toFixed(0) + ' spectra/s'
                + '; render ' + (this.renderMs / Math.max(1, this.paintedFrames)).toFixed(2) + ' ms'
                + '; history ' + (this.historyMs / Math.max(1, this.acquiredFrames)).toFixed(2) + ' ms'
                + '; frame wait ' + (this.waitMs / Math.max(1, this.paintedFrames)).toFixed(1) + ' ms';
        }
        this.rateStarted = performance.now();
        this.renderedFrames = this.paintedFrames = this.acquiredFrames = 0;
        this.renderMs = this.historyMs = this.waitMs = 0;
    }

    async updatePlot(): Promise<void> {
        if (!this.running || this.paused || this.document.hidden || this.busy) { return; }
        this.busy = true;
        const started = performance.now();
        let delay = 1000 / 60;
        try {
            const frame = typeof this.fft.readSpectrum === 'function' ? await this.fft.readSpectrum()
                : {psd: await this.fft.read_psd(), status: this.fft.status};
            if (!this.running || this.paused || this.document.hidden) { return; }
            if (frame) { this.acceptSpectrum(frame.psd, performance.now() / 1000, frame.status); }
            else if (this.fft.waitingForSpectrum) { this.setStatus('connecting', 'Waiting for fresh spectrum…'); }
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

    private acceptSpectrum(psd: Float32Array, time: number, frameStatus: IFFTStatus = this.fft.status): void {
        if (!psd.some(value => Number.isFinite(value) && value > 0)) {
            this.setStatus('connecting', 'Waiting for spectrum…'); return;
        }
        const status = {...frameStatus, dds_freq: frameStatus.dds_freq.slice()};
        const historyStarted = performance.now();
        if (this.history) { this.history.add(psd, status, time); }
        this.historyMs += performance.now() - historyStarted;
        this.acquiredFrames++;
        if (!this.paintFailed && !this.spectrumChanged(psd) && this.sameFrameStatus(status) && this.view === 'spectrum' &&
            !(this.document.getElementById('average-trace') as HTMLInputElement).checked &&
            !(this.document.getElementById('max-hold-trace') as HTMLInputElement).checked &&
            !this.plotBasics.needsRedraw()) {
            // The newest reply matches the displayed frame. Discard an older
            // waiting frame too, while retaining every receipt in history.
            this.pending = undefined;
            this.setStatus('live', 'Live spectrum');
            return;
        }
        this.pending = {psd: psd.slice(), status};
        this.requestDraw();
    }

    private spectrumChanged(next: Float32Array): boolean {
        if (!this.psd || next.length !== this.psd.length) { return true; }
        for (let i = 0; i < next.length; i++) {
            if (!Object.is(next[i], this.psd[i])) { return true; }
        }
        return false;
    }

    private sameFrameStatus(status: IFFTStatus): boolean {
        return !!this.frameStatus &&
            (['fs', 'channel', 'window_index', 'W1', 'W2', 'clkIndex', 'acquisitionKey'] as (keyof IFFTStatus)[])
                .every(key => Object.is(status[key], this.frameStatus[key])) &&
            status.dds_freq.length === this.frameStatus.dds_freq.length &&
            status.dds_freq.every((value, i) => Object.is(value, this.frameStatus.dds_freq[i]));
    }

    private displaySpectrum(): void {
        this.unit = this.document.querySelector<HTMLInputElement>('.unit-input:checked').value;
        this.yLabel = this.unit === 'dBV' ? 'Voltage (dBV)' : this.unit === 'dbv-rtHz' ? 'Voltage noise (dBV/√Hz)' : this.unit === 'dBm-Hz' ? 'PSD (dBm/Hz)' : this.unit === 'dBm' ? 'Power (dBm)' : 'Voltage noise (nV/√Hz)';
        const fs = this.frameStatus.fs;
        if (fs !== this.samplingFrequency) {
            this.samplingFrequency = fs;
            this.plotBasics.setRangeX(this.frameStatus.spectrum ? 10 : 0, this.frameStatus.spectrum ? fs / 2 : fs / 2e6);
        }
        // Bin k is at k * fs / FFT size. The server returns N/2 bins,
        // including DC and excluding Nyquist; never add a synthetic tail bin.
        const length = Math.min(this.n_pts, this.psd.length);
        this.plot_data.length = length;
        for (let i = 0; i < length; i++) {
            const row = this.plot_data[i] || (this.plot_data[i] = [0, 0]);
            row[0] = this.frequencyAt(i, this.frameStatus);
            row[1] = this.convertValue(this.psd[i], this.unit, this.frameStatus, i);
        }
        this.document.getElementById('bin-spacing').textContent = this.frameStatus.spectrum?.binSpacings
            ? this.frameStatus.spectrum.binSpacings.map(step => Number((step < 1000 ? step : step / 1000).toPrecision(3)) + (step < 1000 ? ' Hz' : ' kHz')).join(' / ')
            : (fs / this.fft.fft_size / 1000).toFixed(3) + ' kHz';
        this.document.getElementById('fft-size').textContent = this.fft.fft_size.toLocaleString() + ' points';
        // A captured frame is immutable: convert it only after capture or a unit change.
        if (this.reference && (!this.reference_data || this.referenceUnit !== this.unit)) {
            this.reference_data = Array.from(this.reference.psd, (value, index) => [
                this.frequencyAt(index, this.reference.status),
                this.convertValue(value, this.unit, this.reference.status, index)
            ]);
            this.referenceUnit = this.unit;
        }
        const convertTrace = (values: Float32Array, data: number[][]): number[][] => {
            if (!values) { return undefined; }
            data = data || []; data.length = values.length;
            for (let i = 0; i < values.length; i++) {
                const row = data[i] || (data[i] = [0, 0]);
                row[0] = this.frequencyAt(i, this.history.status);
                row[1] = this.convertValue(values[i], this.unit, this.history.status, i);
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
        this.document.getElementById('reference-status').textContent = this.channelLabel(this.reference.status)
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
        this.document.getElementById('peak-frequency').textContent = peak.length ? (this.frequencyUnit === 'Hz' ? Number(peak[0].toPrecision(7)).toLocaleString() : peak[0].toFixed(6)) + ' ' + this.frequencyUnit : '—';
        const unitLabel = this.unit === 'dBV' ? 'dBV' : this.unit === 'dbv-rtHz' ? 'dBV/√Hz' : this.unit === 'dBm-Hz' ? 'dBm/Hz' : this.unit === 'dBm' ? 'dBm' : 'nV/√Hz';
        this.document.getElementById('peak-level').textContent = peak.length ? peak[1].toFixed(2) + ' ' + unitLabel : '—';
        this.peak = peak;
        if (this.view !== 'spectrum') { this.views.render(this.unit, this.yLabel, this.paused); return; }
        const traces: {label: string; color: string; data: number[][]}[] = [];
        if (this.average_data) { traces.push({label: 'Average', color: '#389168', data: this.average_data}); }
        if (this.maximum_data) { traces.push({label: 'Max hold', color: '#ba861a', data: this.maximum_data}); }
        this.plotBasics.redraw(this.plot_data, this.plot_data.length, this.peak.slice(), this.yLabel, () => {}, this.reference_data, true, traces);
    }

    convertValue(value: number, unit: string, status: IFFTStatus = this.frameStatus || this.fft.status, index = 0): number {
        if (!Number.isFinite(value) || value < 0) { return NaN; }
        if (status.spectrum) {
            if (unit === 'dbv-rtHz') { return 10 * Math.log10(value); }
            if (unit === 'dBV') { return 10 * Math.log10(value * status.spectrum.bandwidths[index]); }
            return Math.sqrt(value) * 1e9;
        }
        if (unit === 'dBm-Hz') { return 10 * Math.log10(value / 1e-3); }
        if (unit === 'dBm') {
            return 10 * Math.log10(value * (status.W2 / status.W1) * status.fs / this.fft.fft_size / 1e-3);
        }
        return Math.sqrt(50 * value) * 1e9;
    }

    get frequencyUnit(): string { return (this.frameStatus || this.fft.status).spectrum?.unit || 'MHz'; }

    frequencyAt(index: number, status: IFFTStatus = this.frameStatus): number {
        return status.spectrum ? status.spectrum.frequencies[index] : index * status.fs / this.fft.fft_size / 1e6;
    }

    channelLabel(status: IFFTStatus): string {
        return status.spectrum && status.channel >= 2 ? (status.channel === 2 ? 'ADC 0 − 1' : 'ADC 0 + 1') : 'ADC ' + status.channel;
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
        window.removeEventListener('resize', this.resizeHandler);
        this.pending = undefined;
    }
}
