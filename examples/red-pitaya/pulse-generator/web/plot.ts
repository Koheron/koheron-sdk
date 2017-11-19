// Plot widget
// (c) Koheron

class Plot {
    private plot_x_max: number = 1024;
    private min_y: number = -1.0; // Volts
    private max_y: number = 1.0; // Volts

    private nPoints: number;

    private reset_range: boolean;
    private options: jquery.flot.plotOptions;
    private plot: jquery.flot.plot;
    private zoom_x: boolean;
    private zoom_y: boolean;
    private zoom_x_btn: HTMLLinkElement;
    private zoom_y_btn: HTMLLinkElement;

    private range_x: jquery.flot.range;
    private range_y: jquery.flot.range;

    constructor(document: Document,
                private plot_placeholder: JQuery,
                private driver: PulseGenerator) {
        this.setPlot();

        this.range_x = <jquery.flot.range>{};
        this.range_x.from = 0;
        this.range_x.to = this.plot_x_max;

        this.range_y = <jquery.flot.range>{};
        this.range_y.from = this.min_y;
        this.range_y.to = this.max_y;

        this.zoom_x = false;
        this.zoom_y = false;
        this.zoom_x_btn = <HTMLLinkElement>document.getElementById('zoom-x-btn');
        this.zoom_y_btn = <HTMLLinkElement>document.getElementById('zoom-y-btn');

        this.update_plot();
    }

    update_plot() {
        this.driver.getFifoBuffer( (adc0: number[][], adc1: number[][]) => {
            this.redraw(adc0, adc1, () => {
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
                mode: 'xy'
            },
            colors: ['#0022FF', '#006400'],
            legend: {
                show: true,
                noColumns: 0,
                labelFormatter: (label: string, series: any): string => {return '<b>' + label + '\t</b>'},
                margin: 0,
                position: 'ne',
            }
        }

        this.rangeSelect();
        this.dblClick();
        this.onWheel();
        this.reset_range = true;
    }

    rangeSelect() {
        this.plot_placeholder.bind('plotselected', (event: JQueryEventObject, ranges: jquery.flot.ranges) => {
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
        this.plot_placeholder.bind('dblclick', (evt: JQueryEventObject) => {
            this.range_x.from = 0;
            this.range_x.to = this.plot_x_max;

            this.range_y = <jquery.flot.range>{};
            this.resetRange();
        });
    }

    onWheel() {
        this.plot_placeholder.bind('wheel', (evt: JQueryEventObject) => {
            let delta: number = (<JQueryMousewheel.JQueryMousewheelEventObject>evt.originalEvent).deltaX
                                + (<JQueryMousewheel.JQueryMousewheelEventObject>evt.originalEvent).deltaY;
            delta /= Math.abs(delta);

            const zoom_ratio: number = 0.2;

            if ((<JQueryInputEventObject>evt.originalEvent).shiftKey || this.zoom_y == true) { // Zoom Y
                const position_y_px: number = (<JQueryMouseEventObject>evt.originalEvent).pageY - this.plot.offset().top;
                const y0: any = this.plot.getAxes().yaxis.c2p(<any>position_y_px);

                this.range_y.from = y0 - (1 + zoom_ratio * delta) * (y0 - this.plot.getAxes().yaxis.min);
                this.range_y.to = y0 - (1 + zoom_ratio * delta) * (y0 - this.plot.getAxes().yaxis.max);

                this.resetRange();
            } else if ((<JQueryInputEventObject>evt.originalEvent).altKey || this.zoom_x == true) { // Zoom X
                const position_x_px: number = (<JQueryMouseEventObject>evt.originalEvent).pageX - this.plot.offset().left;
                const t0: any = this.plot.getAxes().xaxis.c2p(<any>position_x_px);

                if (t0 < 0 || t0  > this.plot_x_max) {
                    return;
                }

                this.range_x.from = Math.max(t0 - (1 + zoom_ratio * delta) * (t0 - this.plot.getAxes().xaxis.min), 0);
                this.range_x.to = Math.min(t0 - (1 + zoom_ratio * delta) * (t0 - this.plot.getAxes().xaxis.max), this.plot_x_max);

                this.resetRange();
            }
        });
    }

    disableWindowWheel() {
        $(window).bind('wheel', (event: JQueryEventObject) => {
            (<JQueryMousewheel.JQueryMousewheelEventObject>event).preventDefault();
        })
    }

    enableWindowWheel() {
        $(window).unbind('wheel');
    }

    zoomX() {
        if (this.zoom_x == false) {
            this.zoom_x = true;
            this.disableWindowWheel();
            if (this.zoom_y) {
                this.zoom_y = false;
                this.zoom_y_btn.className = 'btn btn-default-reversed';
            }
            this.zoom_x_btn.className = 'btn btn-default-reversed active';
        }
        else if (this.zoom_x) {
            this.zoom_x = false;
            this.enableWindowWheel();
            this.zoom_x_btn.className = 'btn btn-default-reversed';

        }
    }

    zoomY() {
        if (this.zoom_y == false) {
            this.zoom_y = true;
            this.disableWindowWheel();
            if (this.zoom_x == true){
                this.zoom_x = false;
                this.zoom_x_btn.className = 'btn btn-default-reversed';
            }
            this.zoom_y_btn.className = 'btn btn-default-reversed active';
        }
        else if (this.zoom_y) {
            this.zoom_y = false;
            this.enableWindowWheel();
            this.zoom_y_btn.className = 'btn btn-default-reversed';
        }
    }

    autoscale() {
        this.range_x.from = 0;
        this.range_x.to = this.plot_x_max;

        this.range_y = <jquery.flot.range>{};
        this.resetRange();
    }

    resetRange() {
        this.reset_range = true;
    }

    redraw(channel0: number[][], channel1: number[][], callback: () => void) {
        if (channel0.length === 0 || channel1.length === 0) {
            callback();
            return;
        }

        const plt_data: jquery.flot.dataSeries[]
                = [{label: 'ADC 1', data: channel0},
                   {label: 'ADC 2', data: channel1}];

        if (this.reset_range) {
            this.plot_x_max = channel0.length;

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