// Plot widget
// (c) Koheron

class Plot {
    public n_pts: number;
    public plot: jquery.flot.plot;
    public plot_data: Array<Array<number>>;

    public yLabel: string = "Power Spectral Density";
    private peakDatapoint: number[];

    constructor(document: Document, private fft: FFT, private plotBasics: PlotBasics) {
        this.n_pts = this.fft.fft_size / 2;
        this.peakDatapoint = [];
        this.plot_data = [];

        this.plotBasics.enableDecimation();
        this.updatePlot();
    }

    private _busy = false;
    private _targetHz = 60;
    private _lastTick = 0;
    private _xAxisReady = false;

    private samplingFrequency = 0;   // cache fs used to build X axis
    private _axisPts = 0;            // cache n_pts used to build X axis

    // Ensure plot_data is allocated once and reused.
    // We keep X fixed (when possible) and only update Y per frame.
    private ensurePlotBuffer() {
        const N = this.n_pts + 1;
        if (!this.plot_data || this.plot_data.length !== N) {
            this.plot_data = new Array(N);
            for (let i = 0; i < N; i++) this.plot_data[i] = [0, NaN];
            this._xAxisReady = false; // X needs a rebuild for the new size
        }
    }

    // (Re)build X axis only when fs or N changed.
    // Also updates plotBasics range and peakDatapoint X.
    private setFreqAxis() {
        this.ensurePlotBuffer();

        const fs = this.fft.status.fs;          // Hz
        const N = this.n_pts;
        const xMaxMHz = fs / 1e6 / 2;           // MHz

        // Only rebuild if fs or N changed
        if (fs !== this.samplingFrequency || N !== this._axisPts || !this._xAxisReady) {
            this.samplingFrequency = fs;
            this._axisPts = N;

            // update plotBasics and x range
            this.plotBasics.x_max = xMaxMHz;
            this.plotBasics.setRangeX(0, xMaxMHz);

            // fill X once: freq_i = (i+1) * xMax / N, keeping your original convention
            const invN = 1 / N;
            for (let i = 0; i <= N; i++) {
                const freq = (i + 1) * xMaxMHz * invN;
                this.plot_data[i][0] = freq;
            }

            this._xAxisReady = true;
        }

        // keep peak X (first bin center in your original code)
        this.peakDatapoint = [xMaxMHz / this.n_pts, this.peakDatapoint?.[1] ?? NaN];
    }

    // Main loop
    async updatePlot() {
        // prevent overlapping frames
        if (this._busy) return;
        this._busy = true;

        const frameBudgetMs = 1000 / this._targetHz;
        const now = performance.now();
        const sinceLast = now - this._lastTick;

        // throttle
        if (sinceLast < frameBudgetMs) {
            this._busy = false;
            const wait = Math.ceil(frameBudgetMs - sinceLast);
            setTimeout(() => requestAnimationFrame(() => this.updatePlot()), wait);
            return;
        }
        this._lastTick = now;

        try {
            // grab PSD
            const psd: Float32Array | number[] = await this.fft.read_psd();

            // refresh X axis only if needed
            this.setFreqAxis();

            // read unit once per frame
            const unitInput = document.querySelector<HTMLInputElement>(".unit-input:checked");
            const yUnit = unitInput ? unitInput.value : "";

            // update Y values in-place; keep X as-is
            const N = Math.min(this.n_pts, psd.length - 1);
            for (let i = 0; i <= N; i++) {
                this.plot_data[i][1] = this.convertValue(psd[i], yUnit);
            }
            // if psd shorter than buffer, pad tail with NaN (avoids old data showing)
            for (let i = N + 1; i <= this.n_pts; i++) {
                this.plot_data[i][1] = NaN;
            }

            // update peak point Y (X already set in setFreqAxis)
            this.peakDatapoint[1] = this.convertValue(psd[0], yUnit);

            // draw; schedule next only after redraw finishes
            this.plotBasics.redraw(
                this.plot_data,
                this.n_pts,
                this.peakDatapoint,
                this.yLabel,
                () => {
                    this._busy = false;

                    // account for redraw time to keep near targetHz
                    const elapsed = performance.now() - now;
                    const delay = Math.max(0, Math.ceil(frameBudgetMs - elapsed));
                    setTimeout(() => requestAnimationFrame(() => this.updatePlot()), delay);
                }
            );
        } catch (err) {
            console.error("updatePlot error:", err);
            this._busy = false;
            // backoff a bit on error
            setTimeout(() => requestAnimationFrame(() => this.updatePlot()), 500);
        }
    }

    convertValue(inValue: number, outUnit: string): number {
        // inValue in W / Hz
        let outValue: number = 0;

        if (outUnit === "dBm-Hz") {
            outValue = 10 * Math.log(inValue / 1E-3) / Math.LN10;
        } else if (outUnit === "dBm") {
            outValue = 10 * Math.log(inValue * (this.fft.status.W2 / this.fft.status.W1) * this.fft.status.fs / this.fft.fft_size / 1E-3) / Math.LN10;
        } else if (outUnit === "nv-rtHz") {
            outValue = Math.sqrt(50 * inValue) * 1E9;
        }

        return outValue;
    }
}
