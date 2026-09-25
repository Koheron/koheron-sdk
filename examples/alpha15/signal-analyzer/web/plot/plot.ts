// Plot widget
// (c) Koheron

type PsdArray = Float32Array | Float64Array;
type PsdProvider = () => Promise<PsdArray>;

interface SegmentConfig {
  name: string;
  fetchPsd: PsdProvider; // Fetch PSD data for this segment
  getFs: () => number; // Sampling frequency (Hz)
  getNBins: () => number; // Number of bins in the PSD array
  getBinWidth: () => number; // Frequency spacing between adjacent PSD bins

  skipFirst: number;           // how many bins to drop from DC side
  skipLast: number;            // how many bins to drop from Nyquist side

  // Optional: whether to place peak marker using this segment (first true wins)
  markPeak?: boolean;
}

// After computing layout we also carry indices & placement
interface SegmentLayout extends SegmentConfig {
    iStart: number;
    iEnd: number;
    size: number;
    offset: number;
}

class Plot {
    public plot: jquery.flot.plot;
    public plot_data: Array<Array<number>> = [];
    public yLabel = "Power Spectrum";
    private peakDatapoint: number[] = [];
    
    private segs: SegmentLayout[] = [];
    private total_pts = 0;

    // axis unit state
    private yunit = "dBV";

    constructor(
        private document: Document,
        private fft: FFT,
        private decimator: Decimator,
        private plotBasics: PlotBasics
    ) {
            // Plot segments
            const segConfigs: SegmentConfig[] = [
                {
                    name: "low-freq",
                    fetchPsd: () => this.decimator.spectralDensityLf(),
                    getFs: () => this.decimator.status.fs_lf,
                    getNBins: () => 1 + this.decimator.status.n_pts / 2,
                    getBinWidth: () => this.decimator.status.fs_lf / this.decimator.status.n_pts,
                    skipFirst: 1, // DC cannot be displayed on a logarithmic axis.
                    skipLast: 577, // End before the mid-frequency segment begins.
                    markPeak: true,
                },
                {
                    name: "mid-freq",
                    fetchPsd: () => this.decimator.spectralDensity(),
                    getFs: () => this.decimator.status.fs,
                    getNBins: () => 1 + this.decimator.status.n_pts / 2,
                    getBinWidth: () => this.decimator.status.fs / this.decimator.status.n_pts,
                    skipFirst: 220,
                    skipLast: 600,
                },
                {
                    name: "high-freq",
                    fetchPsd: () => this.fft.readPsd(),
                    getFs: () => this.fft.status.fs,
                    getNBins: () => this.fft.fft_size / 2,
                    getBinWidth: () => this.fft.status.fs / this.fft.fft_size,
                    skipFirst: 110,
                    skipLast: 0,
                },
            ];

            this.segs = this.computeLayout(segConfigs);
            this.total_pts = this.segs.reduce((s, e) => s + e.size, 0);

            this.plotBasics.LogYaxisFormatter = (val, axis) => {
                if (val >= 1e9) return (val / 1e9).toFixed(axis.tickDecimals);
                if (val >= 1e6) return (val / 1e6).toFixed(axis.tickDecimals) + "m";
                if (val >= 1e3) return (val / 1e3).toFixed(axis.tickDecimals) + "µ";
                return val.toFixed(axis.tickDecimals) + "n";
            };

            this.plotBasics.datapointFormatter = ([frequency, value]) => {
                if (this.yunit === "v-rtHz") {
                    return `(${frequency.toFixed(2)} Hz, ${(value / 1e9).toPrecision(3)} V/√Hz)`;
                }
                const unit = this.yunit === "dbv-rtHz" ? "dBV/√Hz" : "dBV";
                return `(${frequency.toFixed(2)} Hz, ${value.toFixed(2)} ${unit})`;
            };

            this.plotBasics.x_max = this.fft.status.fs / 2;
            this.plotBasics.setRangeX(10.0, this.plotBasics.x_max);
            this.plotBasics.setLogX();
            this.plotBasics.enableDecimation();

            this.updatePlot();
        }

    private computeLayout(configs: SegmentConfig[]): SegmentLayout[] {
        let offset = 0;
        return configs.map<SegmentLayout>(cfg => {
            const nBins = cfg.getNBins();
            const iStart = clamp(cfg.skipFirst, 0, Math.max(0, nBins - 1));
            const iEnd   = clamp(nBins - 1 - cfg.skipLast, iStart, nBins - 1);
            const size   = iEnd - iStart + 1;

            const entry: SegmentLayout = { ...cfg, iStart, iEnd, size, offset };
            offset += size;
            return entry;
        });
    }

    private async updatePlot() {
        // Fetch all PSDs in parallel
        // TODO Update each segment at its acquisition rate
        const psdPromises = this.segs.map(s => s.fetchPsd());
        let psds: PsdArray[];
        try {
            psds = await Promise.all(psdPromises);
        } catch (error) {
            const status = this.document.getElementById("stream-status");
            status.textContent = "Disconnected · showing last spectrum";
            status.className = "stream-error";
            this.document.getElementById("reconnect").hidden = false;
            return;
        }

        const yUnit = (this.document.querySelector(".unit-input:checked") as HTMLInputElement).value;
        if (this.yunit !== yUnit) {
            this.yunit = yUnit;
            if (yUnit === "v-rtHz") {
                this.plotBasics.setLogY();
            } else {
                this.plotBasics.setLinY();
            }
        }

        let peakSet = false;

        for (let si = 0; si < this.segs.length; si++) {
            const seg = this.segs[si];
            const psd = psds[si];
            const fs  = seg.getFs();

            const fstep = seg.getBinWidth();
            if (!peakSet && seg.markPeak && seg.iStart + 3 <= seg.iEnd) {
                this.peakDatapoint = [(seg.iStart + 3) * fstep, this.convertValue(psd[seg.iStart + 3], fs)];
                peakSet = true;
            }

            for (let i = seg.iStart, local = 0; i <= seg.iEnd; ++i, ++local) {
                const freq = i * fstep;
                const y    = this.convertValue(psd[i], fs);
                this.plot_data[seg.offset + local] = [freq, y];
            }
        }

        this.updateReadout();
        this.plotBasics.redraw(
            this.plot_data,
            this.total_pts,
            this.peakDatapoint,
            this.yLabel,
            () => requestAnimationFrame(() => { this.updatePlot(); })
        );
    }

    private updateReadout(): void {
        const status = this.document.getElementById("stream-status");
        status.textContent = "Live spectrum";
        status.className = "stream-live";
        let peak: number[] = null;
        for (const point of this.plot_data) {
            if (point[0] >= 10 && isFinite(point[1]) && (!peak || point[1] > peak[1])) peak = point;
        }
        const density = this.yunit !== "dBV";
        this.document.getElementById("peak-level-label").textContent = density ? "Peak density" : "Peak level";
        this.document.getElementById("unit-description").textContent = this.yunit === "dBV"
            ? "Level per FFT bin. Noise levels depend on each frequency band's resolution bandwidth."
            : "Density per √Hz. Use this view to compare noise across frequency bands.";
        if (peak) {
            this.document.getElementById("peak-frequency").textContent = peak[0] >= 1000
                ? (peak[0] / 1000).toFixed(3) + " kHz" : peak[0].toFixed(2) + " Hz";
            let level: string;
            if (this.yunit === "v-rtHz") {
                const volts = peak[1] / 1e9;
                level = volts >= 1 ? volts.toPrecision(4) + " V/√Hz"
                    : volts >= 1e-3 ? (volts * 1e3).toPrecision(4) + " mV/√Hz"
                    : volts >= 1e-6 ? (volts * 1e6).toPrecision(4) + " µV/√Hz"
                    : (volts * 1e9).toPrecision(4) + " nV/√Hz";
            } else {
                level = peak[1].toFixed(2) + (density ? " dBV/√Hz" : " dBV");
            }
            this.document.getElementById("peak-level").textContent = level;
        }
        const channel = this.fft.status.channel;
        const names = ["ADC 0", "ADC 1", "ADC 0 − ADC 1", "ADC 0 + ADC 1"];
        const windowSelect = <HTMLSelectElement>this.document.getElementById("window");
        this.document.getElementById("measurement-context").textContent =
            (names[channel] || "ADC") + " · " + windowSelect.options[windowSelect.selectedIndex].text;
    }

    private convertValue(value: number, fs: number): number {
        // value in V^2 / Hz
        if (this.yunit === "dBV")         return 10 * Math.log10(value * this.fft.status.ENBW * fs);
        if (this.yunit === "dbv-rtHz")    return 10 * Math.log10(value);
        /* v-rtHz */                      return Math.sqrt(value) * 1e9;
    }
} // class Plot

// Helpers
function clamp(x: number, lo: number, hi: number) {
  return Math.min(Math.max(x, lo), hi);
}
