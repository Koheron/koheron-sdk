// Plot widget
// (c) Koheron

class Plot {
    private n_pts_slow: number;
    private n_pts_fast: number;
    public n_pts: number;

    private n_start_fast: number;
    private n_start_slow: number;
    private n_stop_slow: number;
    private n0_slow: number;
    private n0_fast: number;

    public plot: jquery.flot.plot;
    public plot_data: Array<Array<number>>;

    public yLabel: string = "Power Spectrum";
    private peakDatapoint: number[];

    private yunit: string;

    constructor(document: Document,
                private fft: FFT,
                private decimator: Decimator,
                private plotBasics: PlotBasics) {
        this.n_pts_fast = this.fft.fft_size / 2;
        this.n_pts_slow = 1 + this.decimator.status.n_pts / 2;
        this.n_pts = this.n_pts_slow + this.n_pts_fast;

        this.n_start_slow = 1;
        this.n_stop_slow = 600;
        this.n_start_fast = 55;

        let size_slow = this.n_pts_slow - this.n_stop_slow - this.n_start_slow;

        this.n0_slow = 0;
        this.n0_fast = size_slow + 1;
        
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
        this.plotBasics.setRangeX(100, this.plotBasics.x_max);
        this.plotBasics.setLogX();

        this.updatePlot();
    }

    updatePlot() {
        this.fft.read_psd( (fast_psd: Float32Array) => {
            this.decimator.spectral_density( (slow_psd: Float64Array) => {
                let yUnit: string = (<HTMLInputElement>document.querySelector(".unit-input:checked")).value;

                if (this.yunit !== yUnit) {
                    this.yunit = yUnit;

                    if (yUnit === "v-rtHz") {
                        this.plotBasics.setLogY();
                    } else {
                        this.plotBasics.setLinY();
                    }
                }

                this.updateSlowPsdPlot(slow_psd);
                this.updateFastPsdPlot(fast_psd);

                this.plotBasics.redraw(this.plot_data,
                                       this.n_pts,
                                       this.peakDatapoint,
                                       this.yLabel, () => {
                    requestAnimationFrame( () => { this.updatePlot(); } );
                });
            });
        });
    }

    private updateSlowPsdPlot(slow_psd: Float64Array) {
        let fstep = this.decimator.status.fs / 2 / this.n_pts_slow;

        this.peakDatapoint = [4 * fstep, this.convertValue(slow_psd[3], this.decimator.status.fs)];

        for (let i: number = this.n_start_slow; i <= this.n_pts_slow - this.n_stop_slow; i++) {
            let freq = (i + 1) * fstep;
            let convertedSlowPsd = this.convertValue(slow_psd[i], this.decimator.status.fs);
            this.plot_data[this.n0_slow - this.n_start_slow + i] = [freq, convertedSlowPsd];
        }
    }

    private updateFastPsdPlot(fast_psd: Float32Array) {
        let fstep = this.plotBasics.x_max / this.n_pts_fast;

        for (let i: number = this.n_start_fast; i <= this.n_pts_fast; i++) {
            let freq = (i + 1) * fstep;
            let convertedFastPsd = this.convertValue(fast_psd[i], this.fft.status.fs);
            this.plot_data[this.n0_fast - this.n_start_fast + i] = [freq, convertedFastPsd];
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
