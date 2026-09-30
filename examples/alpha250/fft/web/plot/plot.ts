// (c) Koheron

// Spectrum display. Hardware acquisition continues when the display is paused.
class Plot {
    private running = true;
    private paused = false;
    private busy = false;
    private timer: number;
    private animation: number;
    private samplingFrequency = 0;
    private peak: number[] = [];
    private psd: Float32Array;
    private reference: {psd: Float32Array; status: IFFTStatus};
    public reference_data: number[][];
    public get referenceStatus(): IFFTStatus { return this.reference && this.reference.status; }
    public n_pts: number;
    public plot_data: number[][] = [];
    public yLabel = 'PSD (dBm/Hz)';
    public unit = 'dBm-Hz';
    public frameStatus: IFFTStatus;

    constructor(private document: Document, private fft: FFT, private plotBasics: PlotBasics) {
        this.n_pts = fft.fft_size / 2;
        // Keep every bin for reliable initial auto-scaling and cursor selection.
        this.plotBasics.disableDecimation();
        this.plotBasics.setLinY();
        for (const input of Array.from(document.querySelectorAll<HTMLInputElement>('.unit-input'))) {
            input.addEventListener('change', () => {
                this.plotBasics.setLinY();
                if (this.psd) { this.displaySpectrum(); }
            });
        }
        document.getElementById('exclude-dc').addEventListener('change', () => this.redraw());
        document.getElementById('capture-reference').addEventListener('click', () => this.captureReference());
        document.getElementById('clear-reference').addEventListener('click', () => this.clearReference());
        document.querySelector('.peak-input').addEventListener('change', () => {
            if (this.plot_data.length) { this.redraw(); }
        });
        $('#plot-placeholder').on('plotselected.fft dblclick.fft wheel.fft', () => {
            if (this.plot_data.length) { this.redraw(); }
        });
        this.updatePlot();
    }

    setPaused(paused: boolean): void {
        this.paused = paused;
        this.setStatus(paused ? 'paused' : 'connecting', paused ? 'Display paused' : 'Resuming…');
        if (!paused) { this.updatePlot(); }
    }

    private setStatus(state: string, text: string): void {
        const status = this.document.getElementById('connection-status');
        status.dataset.state = state;
        status.textContent = text;
    }

    private schedule(delay: number): void {
        window.clearTimeout(this.timer);
        window.cancelAnimationFrame(this.animation);
        if (!this.running || this.paused) { return; }
        this.timer = window.setTimeout(() => {
            this.animation = window.requestAnimationFrame(() => this.updatePlot());
        }, delay);
    }

    async updatePlot(): Promise<void> {
        if (!this.running || this.paused || this.busy) { return; }
        this.busy = true;
        let delay = 50;
        try {
            const psd = await this.fft.read_psd();
            if (!this.running || this.paused) { return; }
            // The accumulator can return zeros before its first complete frame.
            // Do not fix the automatic Y range from an entirely nonfinite dB plot.
            if (!psd.some(value => Number.isFinite(value) && value > 0)) {
                this.setStatus('connecting', 'Waiting for spectrum…');
                return;
            }
            this.frameStatus = {...this.fft.status, dds_freq: this.fft.status.dds_freq.slice()};
            // Own the displayed samples so unit changes also work while paused.
            this.psd = psd.slice();
            this.displaySpectrum();
            this.setStatus('live', 'Live spectrum');
        } catch (error) {
            if (!this.running || this.paused) { return; }
            this.setStatus('error', 'Waiting for spectrum…');
            console.error('Spectrum update failed:', error);
            delay = 1000;
        } finally {
            this.busy = false;
            this.schedule(delay);
        }
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
        this.reference_data = this.reference && Array.from(this.reference.psd, (value, index) => [
            index * this.reference.status.fs / this.fft.fft_size / 1e6,
            this.convertValue(value, this.unit, this.reference.status)
        ]);
        this.redraw();
        (this.document.getElementById('capture-reference') as HTMLButtonElement).disabled = length === 0;
        for (const button of Array.from(this.document.querySelectorAll<HTMLButtonElement>('.export-data, .export-plot'))) {
            button.disabled = length === 0;
        }
    }

    captureReference(): void {
        if (!this.psd || !this.frameStatus) { return; }
        this.reference = {psd: this.psd.slice(0, this.plot_data.length), status: {...this.frameStatus, dds_freq: this.frameStatus.dds_freq.slice()}};
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
        this.plotBasics.redraw(this.plot_data, this.plot_data.length, this.peak.slice(), this.yLabel, () => {}, this.reference_data, true);
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
        $('#plot-placeholder').off('.fft');
        window.clearTimeout(this.timer);
        window.cancelAnimationFrame(this.animation);
    }
}
