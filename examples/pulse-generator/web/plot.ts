// Plot widget
// (c) Koheron

class Plot {

    private n_points: number;

    private n_pts : number = 512;

    private minX: number = 0;
    private maxX: number = 62.5;
    private xRange: jquery.flot.range;

    private minY: number = -200;
    private maxY: number = 170;
    private yRange: jquery.flot.range;

    private isResetRange: boolean;
    private options: jquery.flot.plotOptions;
    private plot: jquery.flot.plot;

    constructor(document: Document,
                private plot_placeholder: JQuery,
                private driver: PulseGenerator) {
        this.setPlot();

        this.xRange = <jquery.flot.range>{};
        this.xRange.from = this.minX;
        this.xRange.to = this.maxX;

        this.yRange = <jquery.flot.range>{};
        this.yRange.from = this.minY;
        this.yRange.to = this.maxY;

        this.n_points = 1024;

        this.updatePlot();
        this.autoScale();
    }

    updatePlot() {
        this.driver.getNextPulse(this.n_points, (adc_data: Uint32Array) => {
            this.redraw(adc_data, () => {
                requestAnimationFrame( () => { this.updatePlot(); } );
            });
        });
    }

    setPlot() {
        this.isResetRange = false;

        this.options = {
            series: {
                shadowSize: 0 // Drawing is faster without shadows
            },
            yaxis: {
                min: this.minY,
                max: this.maxY
            },
            xaxis: {
                min: this.minX,
                max: this.maxX,
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
                show: false,
                noColumns: 0,
                labelFormatter: (label: string, series: any): string => {return '<b>' + label + '\t</b>'},
                margin: 0,
                position: "ne",
            }
        }

        this.rangeSelect();
        this.isResetRange = true;
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

            this.xRange.from = ranges.xaxis.from;
            this.xRange.to = ranges.xaxis.to;

            this.yRange.from = ranges.yaxis.from;
            this.yRange.to = ranges.yaxis.to;

            this.resetRange();
        });
    }

    autoScale() {

        this.xRange.from = this.minX;
        this.xRange.to = this.maxX;

        this.yRange = <jquery.flot.range>{};
        this.resetRange();
    }

    resetRange() {
        this.isResetRange = true;
    }

    redraw(adc_data: Uint32Array, callback: () => void) {
        let plot_data: Array<Array<number>> = [];

        for (var i: number = 0; i <= this.n_pts; i++) {
            plot_data[i] = [i * this.maxX / this.n_pts, ((adc_data[i] % 16384) - 8192) % 16384 + 8192];
        }

        const plt_data: jquery.flot.dataSeries[]
                = [{label: "Power spectral density (dB)", data: plot_data}];

        if (this.resetRange) {
            this.options.xaxis.min = this.xRange.from;
            this.options.xaxis.max = this.xRange.to;
            this.options.yaxis.min = this.yRange.from;
            this.options.yaxis.max = this.yRange.to;

            this.plot = $.plot(this.plot_placeholder, plt_data, this.options);
            this.plot.setupGrid();
            this.isResetRange = false;
        } else {
            this.plot.setData(plt_data);
            this.plot.draw();
        }

        callback();
    }

}