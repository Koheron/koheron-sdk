// Plot widget
// (c) Koheron

type PsdArray = Float32Array | Float64Array;
type PsdProvider = () => Promise<PsdArray>;

interface SegmentConfig {
  name: string;
  fetchPsd: PsdProvider; // Fetch PSD data for this segment
  getFs: () => number; // Sampling frequency (Hz)
  getNBins: () => number; // Number of FFT bins in the PSD array (N/2+1)

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
                    skipFirst: 0,
                    skipLast: 550,
                    markPeak: true,
                },
                {
                    name: "mid-freq",
                    fetchPsd: () => this.decimator.spectralDensity(),
                    getFs: () => this.decimator.status.fs,
                    getNBins: () => 1 + this.decimator.status.n_pts / 2,
                    skipFirst: 220,
                    skipLast: 600,
                },
                {
                    name: "high-freq",
                    fetchPsd: () => this.fft.readPsd(),
                    getFs: () => this.fft.status.fs,
                    getNBins: () => this.fft.fft_size / 2,
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
        const psds = await Promise.all(psdPromises);

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
            const nBins = seg.getNBins();

            const fstep = (fs / 2) / nBins;
            if (!peakSet && seg.markPeak && seg.iStart + 3 <= seg.iEnd) {
                this.peakDatapoint = [4 * fstep, this.convertValue(psd[seg.iStart + 3], fs)];
                peakSet = true;
            }

            for (let i = seg.iStart, local = 0; i <= seg.iEnd; ++i, ++local) {
                const freq = (i + 1) * fstep;
                const y    = this.convertValue(psd[i], fs);
                this.plot_data[seg.offset + local] = [freq, y];
            }
        }

        this.plotBasics.redraw(
            this.plot_data,
            this.total_pts,
            this.peakDatapoint,
            this.yLabel,
            () => requestAnimationFrame(() => { this.updatePlot(); })
        );
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
