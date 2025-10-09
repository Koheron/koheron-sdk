// Plot widget
// (c) Koheron

class Plot {
    private n_pts_low_freq: number;
    private n_pts_mid_freq: number;
    private n_pts_high_freq: number;
    public n_pts: number;

    private n_start_low_freq: number;
    private n_start_high_freq: number;
    private n_start_mid_freq: number;
    private n_stop_low_freq: number;
    private n_stop_mid_freq: number;

    private n0_low_freq: number;
    private n0_mid_freq: number;
    private n0_high_freq: number;

    public plot: jquery.flot.plot;
    public plot_data: Array<Array<number>>;

    public yLabel: string = "Power Spectrum";
    private peakDatapoint: number[];

    private yunit: string;

    constructor(document: Document,
                private fft: FFT,
                private decimator: Decimator,
                private plotBasics: PlotBasics) {
        this.n_pts_high_freq = this.fft.fft_size / 2;
        // low & mid use the same N/2+1 size from the decimator path
        this.n_pts_mid_freq = 1 + this.decimator.status.n_pts / 2;
        this.n_pts_low_freq = this.n_pts_mid_freq;

        // Ranges
        this.n_start_low_freq = 0;        // skip DC bin
        this.n_stop_low_freq  = 550;        // keep full top of low band
        this.n_start_mid_freq = 220;        // skip DC bin
        this.n_stop_mid_freq  = 600;
        this.n_start_high_freq = 110;

        // Sizes actually plotted
        const size_low_freq = this.n_pts_low_freq - this.n_stop_low_freq - this.n_start_low_freq;
        const size_mid_freq = this.n_pts_mid_freq - this.n_stop_mid_freq - this.n_start_mid_freq;
        const size_high_freq = this.n_pts_high_freq - this.n_start_high_freq + 1;

        // Offsets in plot_data
        this.n0_low_freq = 0;
        this.n0_mid_freq = size_low_freq + 1;
        this.n0_high_freq = size_low_freq + size_mid_freq + 1;

        // Total plotted points
        this.n_pts = size_low_freq + size_mid_freq + size_high_freq;

        this.peakDatapoint = [];
        this.plot_data = [];

        this.yunit = "dBV";
        
        this.plotBasics.LogYaxisFormatter = (val, axis) => {
            if (val >= 1E9) {
                return (val / 1E9).toFixed(axis.tickDecimals);
            } else if (val >= 1E6) {
                return (val / 1E6).toFixed(axis.tickDecimals) + "m";
            } else if (val >= 1E3) {
                return (val / 1E3).toFixed(axis.tickDecimals) + "µ";
            } else {
                return val.toFixed(axis.tickDecimals) + "n";
            }
        }

        this.plotBasics.x_max = this.fft.status.fs / 2;
        this.plotBasics.setRangeX(10.0, this.plotBasics.x_max);
        this.plotBasics.setLogX();
        this.plotBasics.enableDecimation();

        this.updatePlot();
    }

    async updatePlot() {
        const high_freq_psd = await this.fft.readPsd();
        const mid_freq_psd  = await this.decimator.spectralDensity();
        const low_freq_psd  = await this.decimator.spectralDensityLf();

        let yUnit: string = (<HTMLInputElement>document.querySelector(".unit-input:checked")).value;

        if (this.yunit !== yUnit) {
            this.yunit = yUnit;

            if (yUnit === "v-rtHz") {
                this.plotBasics.setLogY();
            } else {
                this.plotBasics.setLinY();
            }
        }

        this.updateLowFreqPsdPlot(low_freq_psd);
        this.updateMidFreqPsdPlot(mid_freq_psd);
        this.updateHighFreqPsdPlot(high_freq_psd);

        this.plotBasics.redraw(this.plot_data,
                               this.n_pts,
                               this.peakDatapoint,
                               this.yLabel, () => {
            requestAnimationFrame( () => { this.updatePlot(); } );
        });
    }

    private updateLowFreqPsdPlot(low_freq_psd: Float64Array) {
        const fs_low = this.decimator.status.fs_lf;
        const fstep = (fs_low / 2) / this.n_pts_low_freq;

        // optional: place peak marker in the low band (here: third bin for consistency)
        this.peakDatapoint = [4 * fstep, this.convertValue(low_freq_psd[3], fs_low)];

        for (let i = this.n_start_low_freq; i <= this.n_pts_low_freq - this.n_stop_low_freq; i++) {
            const freq = (i + 1) * fstep;
            const y = this.convertValue(low_freq_psd[i], fs_low);
            this.plot_data[this.n0_low_freq - this.n_start_low_freq + i] = [freq, y];
        }
    }

    private updateMidFreqPsdPlot(mid_freq_psd: Float64Array) {
        let fstep = this.decimator.status.fs / 2 / this.n_pts_mid_freq;
        this.peakDatapoint = [4 * fstep, this.convertValue(mid_freq_psd[3], this.decimator.status.fs)];

        for (let i: number = this.n_start_mid_freq; i <= this.n_pts_mid_freq - this.n_stop_mid_freq; i++) {
            let freq = (i + 1) * fstep;
            let convertedSlowPsd = this.convertValue(mid_freq_psd[i], this.decimator.status.fs);
            this.plot_data[this.n0_mid_freq - this.n_start_mid_freq + i] = [freq, convertedSlowPsd];
        }
    }

    private updateHighFreqPsdPlot(high_freq_psd: Float32Array) {
        let fstep = this.plotBasics.x_max / this.n_pts_high_freq;

        for (let i: number = this.n_start_high_freq; i <= this.n_pts_high_freq; i++) {
            let freq = (i + 1) * fstep;
            let convertedFastPsd = this.convertValue(high_freq_psd[i], this.fft.status.fs);
            this.plot_data[this.n0_high_freq - this.n_start_high_freq + i] = [freq, convertedFastPsd];
        };
    }

    private convertValue(value: number, fs: number): number {
        // value in V^2 / Hz
        if (this.yunit === "dBV") {
            return 10 * Math.log10(value * this.fft.status.ENBW * fs);
        } else if (this.yunit === "dbv-rtHz") {
            return 10 * Math.log10(value);
        } else if (this.yunit === "v-rtHz") {
            return Math.sqrt(value) * 1E9;
        }
    }
}
