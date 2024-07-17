// Plot widget
// (c) Koheron

class DecimatorPlot {
    public n_pts: number;
    public plot: jquery.flot.plot;
    public plot_data: Array<Array<number>>;

    public yLabel: string = "Voltage (V)";
    private peakDatapoint: number[];

    constructor(document: Document,
                private decimator: Decimator,
                private plotBasics: PlotBasics) {
        this.n_pts = 8192;
        this.peakDatapoint = [];
        this.plot_data = [];

        this.updatePlot();
    }

    updatePlot() {
        this.decimator.read_adc( (data: Float64Array) => {
            for (let i: number = 0; i <= this.n_pts; i++) {
                this.plot_data[i] = [i, data[i]];
            }

            this.plotBasics.redraw(this.plot_data, this.n_pts, this.peakDatapoint, this.yLabel, () => {
                setTimeout(() => { this.updatePlot(); }, 
                           this.decimator.status.tx_duration * 1000);
            });
        });
    }
}
