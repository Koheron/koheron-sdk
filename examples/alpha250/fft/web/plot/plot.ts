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
        document.querySelector('.peak-input').addEventListener('change', () => {
            if (this.paused && this.plot_data.length) { this.redraw(); }
        });
        $('#plot-placeholder').on('plotselected.fft dblclick.fft wheel.fft', () => {
            if (this.paused && this.plot_data.length) { this.redraw(); }
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
        let peak: number[] = [];
        for (let i = 0; i < length; i++) {
            const row = this.plot_data[i] || (this.plot_data[i] = [0, 0]);
            row[0] = i * fs / this.fft.fft_size / 1e6;
            row[1] = this.convertValue(this.psd[i], this.unit);
            if (Number.isFinite(row[1]) && (!peak.length || row[1] > peak[1])) {
                peak = row.slice();
            }
        }
        this.document.getElementById('peak-frequency').textContent = peak.length ? peak[0].toFixed(6) + ' MHz' : '—';
        const unitLabel = this.unit === 'dBm-Hz' ? 'dBm/Hz' : this.unit === 'dBm' ? 'dBm' : 'nV/√Hz';
        this.document.getElementById('peak-level').textContent = peak.length ? peak[1].toFixed(2) + ' ' + unitLabel : '—';
        this.document.getElementById('bin-spacing').textContent = (fs / this.fft.fft_size / 1000).toFixed(3) + ' kHz';
        this.document.getElementById('fft-size').textContent = this.fft.fft_size.toLocaleString() + ' points';
        this.peak = peak;
        this.redraw();
        for (const button of Array.from(this.document.querySelectorAll<HTMLButtonElement>('.export-data, .export-plot'))) {
            button.disabled = length === 0;
        }
    }

    private redraw(): void {
        this.plotBasics.redraw(this.plot_data, this.plot_data.length, this.peak.slice(), this.yLabel, () => {});
    }

    convertValue(value: number, unit: string): number {
        if (!Number.isFinite(value) || value < 0) { return NaN; }
        if (unit === 'dBm-Hz') { return 10 * Math.log10(value / 1e-3); }
        if (unit === 'dBm') {
            const status = this.frameStatus || this.fft.status;
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
