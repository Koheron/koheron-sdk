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

        this.updatePlot();
    }

    updatePlot() {
        this.fft.read_psd( (psd: Float32Array) => {
            let max_x: number = this.fft.status.fs / 1E6 / 2

            if (max_x != this.plotBasics.x_max) { // Sampling frequency has changed
                this.plotBasics.x_max = max_x;
                this.plotBasics.setRangeX(0, this.plotBasics.x_max);
            }

            let yUnit: string = (<HTMLInputElement>document.querySelector(".unit-input:checked")).value;
            this.peakDatapoint = [this.plotBasics.x_max / this.n_pts , this.convertValue(psd[0], yUnit)];

            for (let i: number = 0; i <= this.n_pts; i++) {
                let freq: number = (i + 1) * this.plotBasics.x_max / this.n_pts; // MHz
                let convertedPsd: number = this.convertValue(psd[i], yUnit);
                this.plot_data[i] = [freq, convertedPsd];
            };

            this.plotBasics.redraw(this.plot_data, this.n_pts, this.peakDatapoint, this.yLabel, () => {
                requestAnimationFrame( () => { this.updatePlot(); } );
            });
        });
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
