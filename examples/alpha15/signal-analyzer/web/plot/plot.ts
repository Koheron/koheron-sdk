// Plot widget
// (c) Koheron

class Plot {
    public n_pts_slow: number;
    public n_pts_fast: number;
    public n_pts: number;
    public n_start_fast: number;
    public plot: jquery.flot.plot;
    public plot_data: Array<Array<number>>;

    public yLabel: string = "Power Spectral Density";
    private peakDatapoint: number[];

    constructor(document: Document,
                private fft: FFT,
                private decimator: Decimator,
                private plotBasics: PlotBasics) {
        this.n_pts_fast = this.fft.fft_size / 2;
        this.n_pts_slow = 1 + this.decimator.status.n_pts / 2;
        this.n_start_fast = 20; // Don't plot the 1st 20 points
        this.n_pts = this.n_pts_fast + this.n_pts_slow - this.n_start_fast;
        
        this.peakDatapoint = [];
        this.plot_data = [];

        this.updatePlot();
    }

    updatePlot() {
        this.fft.read_psd( (fast_psd: Float32Array) => {
            this.decimator.spectral_density( (slow_psd: Float64Array) => {
                let yUnit: string = (<HTMLInputElement>document.querySelector(".unit-input:checked")).value;
    
                // Slow PSD
                for (let i: number = 0; i <= this.n_pts_slow; i++) {
                    let freq = (i + 1) * this.decimator.status.fs / 2 / this.n_pts_slow;
                    let convertedSlowPsd = this.convertValue(slow_psd[i], yUnit);
                    this.plot_data[i] = [freq, convertedSlowPsd];
                }

                // Fast PSD
                let max_x: number = this.fft.status.fs / 2

                if (max_x != this.plotBasics.x_max) { // Sampling frequency has changed
                    this.plotBasics.x_max = max_x;
                    this.plotBasics.setRangeX(10, this.plotBasics.x_max);
                    this.plotBasics.setLogX();
                }

                this.peakDatapoint = [this.plotBasics.x_max / this.n_pts_fast ,
                                    this.convertValue(fast_psd[0], yUnit)];

                for (let i: number = this.n_start_fast; i <= this.n_pts_fast; i++) {
                    let freq = (i + 1) * this.plotBasics.x_max / this.n_pts_fast;
                    let convertedFastPsd = this.convertValue(fast_psd[i], yUnit);
                    this.plot_data[this.n_pts_slow + 1 + i - this.n_start_fast] = [freq, convertedFastPsd];
                };

                // Redraw
                this.plotBasics.redraw(this.plot_data, this.n_pts, this.peakDatapoint, this.yLabel, () => {
                    requestAnimationFrame( () => { this.updatePlot(); } );
                });
            });
        });
    }

    private convertValue(inValue: number, outUnit: string): number {
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
