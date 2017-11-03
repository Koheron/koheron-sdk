// Plot widget
// (c) Koheron

class Plot {

    private n_pts : number = 4096;

    private plot_x_max: number = 125;
    private min_y: number = -200;
    private max_y: number = 170;

    private range_x: jquery.flot.range;
    private range_y: jquery.flot.range;

    private reset_range: boolean;
    private options: jquery.flot.plotOptions;
    private plot: jquery.flot.plot;

    constructor(document: Document,
                private plot_placeholder: JQuery,
                private driver: DDS) {
        this.setPlot();

        this.range_x = <jquery.flot.range>{};
        this.range_x.from = 0;
        this.range_x.to = this.plot_x_max;

        this.range_y = <jquery.flot.range>{};
        this.range_y.from = this.min_y;
        this.range_y.to = this.max_y;

        this.update_plot();
    }

    update_plot() {
        this.driver.read_psd( (psd: Float32Array) => {
            this.redraw(psd, () => {
                requestAnimationFrame( () => { this.update_plot(); } );
            });
        });
    }

    setPlot() {
        this.reset_range = false;

        this.options = {
            series: {
                shadowSize: 0 // Drawing is faster without shadows
            },
            yaxis: {
                min: this.min_y,
                max: this.max_y
            },
            xaxis: {
                min: 0,
                max: this.plot_x_max,
                show: true
            },
            grid: {
                margin: {
                    left: 15
                }
            },
            selection: {
                mode: "xy"
            },
            colors: ["#0022FF", "#006400"],
            legend: {
                show: true,
                noColumns: 0,
                labelFormatter: (label: string, series: any): string => {return '<b>' + label + '\t</b>'},
                margin: 0,
                position: "ne",
            }
        }

        this.rangeSelect();
        this.dblClick();
        this.reset_range = true;
    }

    rangeSelect() {
        this.plot_placeholder.bind("plotselected", (event: JQueryEventObject,
                                                    ranges: jquery.flot.ranges) => {
            // Clamp the zooming to prevent external zoom
            if (ranges.xaxis.to - ranges.xaxis.from < 0.00001) {
                ranges.xaxis.to = ranges.xaxis.from + 0.00001;
            }

            if (ranges.yaxis.to - ranges.yaxis.from < 0.00001) {
                ranges.yaxis.to = ranges.yaxis.from + 0.00001;
            }

            this.range_x.from = ranges.xaxis.from;
            this.range_x.to = ranges.xaxis.to;

            this.range_y.from = ranges.yaxis.from;
            this.range_y.to = ranges.yaxis.to;

            this.resetRange();
        });
    }

    // A double click on the plot resets to full span
    dblClick() {
        this.plot_placeholder.bind("dblclick", (evt: JQueryEventObject) => {
            this.range_x.from = 0;
            this.range_x.to = this.plot_x_max;

            this.range_y = <jquery.flot.range>{};
            this.resetRange();
        });
    }

    resetRange() {
        this.reset_range = true;
    }

    redraw(psd: Float32Array, callback: () => void) {
        let plot_data: Array<Array<number>> = [];

        for (var i: number = 0; i <= this.n_pts; i++) {
            let navg: number = 1023;
            let fft_size = 8192;
            let fs: number = 250E6;
            let lpsd_volts: number = Math.sqrt((1 / fs) * psd[i] / navg) / 32768 / 8192 * 1.0385; // 1.12Vpp input range
            let psd_watt = lpsd_volts * lpsd_volts / 50; // W / Hz
            //plot_data[i] = [i * this.plot_x_max / this.n_pts, lpsd_volts * 1E9 ]; // nV/rtHz
            plot_data[i] = [i * this.plot_x_max / this.n_pts, 10 * Math.log(psd_watt / 1E-3) / Math.LN10 ]; // dBm/Hz
            //plot_data[i] = [i * this.plot_x_max / this.n_pts, 10 * Math.log(psd_watt * fs / fft_size / 1E-3) / Math.LN10 ]; // dBm

            //plot_data[i] = [i * this.plot_x_max / this.n_pts, 10 * Math.log(psd[i]) / Math.LN10 ];
        }

        const plt_data: jquery.flot.dataSeries[]
                = [{label: "Power spectral density (dBm/Hz)", data: plot_data}];

        if (this.reset_range) {
            this.options.xaxis.min = this.range_x.from;
            this.options.xaxis.max = this.range_x.to;
            this.options.yaxis.min = this.range_y.from;
            this.options.yaxis.max = this.range_y.to;

            this.plot = $.plot(this.plot_placeholder, plt_data, this.options);
            this.plot.setupGrid();
            this.reset_range = false;
        } else {
            this.plot.setData(plt_data);
            this.plot.draw();
        }

        callback();
    }
}