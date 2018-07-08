// Plot
// (c) Koheron

class Plot {

    private ch1Checkbox: HTMLInputElement;
    private ch2Checkbox: HTMLInputElement;

    private plot: jquery.flot.plot;
    private options: jquery.flot.plotOptions;
    private reset_range: boolean;

    private x_min: number;
    private x_max: number;
    private y_min: number;
    private y_max: number;
    private range_x: jquery.flot.range;
    private range_y: jquery.flot.range;

    private hoverDatapointSpan: HTMLSpanElement;
    private hoverDatapoint: number[];

    private clickDatapointSpan: HTMLSpanElement;
    private clickDatapoint: number[];

    private rangeFunction: string;

    constructor(document: Document, private plot_placeholder: JQuery, private driver) {
        this.ch1Checkbox = <HTMLInputElement>document.getElementById('ch1-checkbox');
        this.ch2Checkbox = <HTMLInputElement>document.getElementById('ch2-checkbox');


        this.rangeFunction = "setTimeRange";

        this.reset_range = false;
        this.setPlot();
        this.rangeSelect(this.rangeFunction);
        this.dblClick(this.rangeFunction);
        this.onWheel();
        this.reset_range = true;

        this.x_min = 0;
        this.x_max = this.driver.maxT;
        this.y_min = -8192;
        this.y_max = +8191;

        this.range_y = {
            from: this.y_min,
            to: this.y_max
        };

        this.range_x = {
            from: this.x_min,
            to: this.x_max
        }

        this.driver[this.rangeFunction](this.range_x);

        this.hoverDatapointSpan = <HTMLSpanElement>document.getElementById("hover-datapoint");
        this.hoverDatapoint = [];

        this.clickDatapointSpan = <HTMLSpanElement>document.getElementById("click-datapoint");
        this.clickDatapoint = [];

        this.updatePlot();

    }

    updatePlot() {

        let is_channel_1: boolean = this.ch1Checkbox.checked;
        let is_channel_2: boolean = this.ch2Checkbox.checked;
        this.driver.getDecimatedData( (ch0: number[][], ch1: number[][], range_x: jquery.flot.range) => {
            this.redraw(ch0, ch1, range_x, "Channel 1", "Channel 2", is_channel_1, is_channel_2, () => {
                requestAnimationFrame( () => {
                    this.updatePlot();
                });
            });
        });
    }

    // == Plot

    setPlot(): void {

        this.options = {
            series: {
                shadowSize: 0 // Drawing is faster without shadows
            },
            yaxis: {
                min: this.y_min,
                max: this.y_max
            },
            xaxis: {
                min: this.x_min,
                max: this.x_max,
                show: true
            },
            grid: {
                margin: {
                    top: 0,
                    left: 0,
                },
                borderColor: "#d5d5d5",
                borderWidth: 1
            },
            selection: {
                mode: 'xy'
            },
            colors: ['#019cd5', '#d53a01']
        }

    }

    rangeSelect(rangeFunction: string): void {
        this.plot_placeholder.bind('plotselected', (event: JQueryEventObject,
                                               ranges: jquery.flot.ranges) => {
            // Clamp the zooming to prevent external zoom
            if (ranges.xaxis.to - ranges.xaxis.from < 0.00001) {
                ranges.xaxis.to = ranges.xaxis.from + 0.00001;
            }

            if (ranges.yaxis.to - ranges.yaxis.from < 0.00001) {
                ranges.yaxis.to = ranges.yaxis.from + 0.00001;
            }

            this.range_y = {
                from: ranges.yaxis.from,
                to: ranges.yaxis.to
            };

            if (rangeFunction.length > 0) {
                this.driver[rangeFunction](ranges.xaxis);
            }

            // this.driver.setTimeRange(ranges.xaxis);
            this.resetRange();
        });
    }

    // A double click on the plot resets to full span
    dblClick(rangeFunction: string): void {
        this.plot_placeholder.bind('dblclick', (evt: JQueryEventObject) => {
            this.range_x = {
                from : 0,
                to   : this.x_max
            };

            this.range_y = <jquery.flot.range>{};

            if (rangeFunction.length > 0) {
                this.driver[rangeFunction](this.range_x);
            }

            this.resetRange();
        });
    }


    onWheel(): void {
        this.plot_placeholder.bind('wheel', (evt: JQueryEventObject) => {
            let delta: number = (<JQueryMousewheel.JQueryMousewheelEventObject>evt.originalEvent).deltaX
                                + (<JQueryMousewheel.JQueryMousewheelEventObject>evt.originalEvent).deltaY;
            delta /= Math.abs(delta);

            const zoomRatio: number = 0.2;

            if ((<JQueryInputEventObject>evt.originalEvent).shiftKey) { // Zoom Y
                const positionY: number = (<JQueryMouseEventObject>evt.originalEvent).pageY - this.plot.offset().top;
                const y0: any = this.plot.getAxes().yaxis.c2p(<any>positionY);

                this.range_y = {
                    from: y0 - (1 + zoomRatio * delta) * (y0 - this.plot.getAxes().yaxis.min),
                    to: y0 - (1 + zoomRatio * delta) * (y0 - this.plot.getAxes().yaxis.max)
                };

                this.resetRange();
                return false;
            } else if ((<JQueryInputEventObject>evt.originalEvent).altKey) { // Zoom X
                const positionX: number = (<JQueryMouseEventObject>evt.originalEvent).pageX - this.plot.offset().left;
                const t0: any = this.plot.getAxes().xaxis.c2p(<any>positionX);

                if (t0 < 0 || t0  > this.x_max) {
                    return;
                }

                // To avoid getting stuck at the edge
                if (this.plot.getAxes().xaxis.max - t0 < 2e6 / this.driver.samplingRate) {
                    delta = 4 * delta;
                }

                this.range_x = {
                    from: Math.max(t0 - (1 + zoomRatio * delta) * (t0 - this.plot.getAxes().xaxis.min), 0),
                    to: Math.min(t0 - (1 + zoomRatio * delta) * (t0 - this.plot.getAxes().xaxis.max), this.x_max)
                };

                this.driver.setTimeRange(this.range_x);
                this.resetRange();
                return false;
            }

            return true
        });
    }

    resetRange(): void {
        this.reset_range = true;
    }

    redraw(ch0: number[][], ch1: number[][],
           rangeX: jquery.flot.range, label1: string, label2: string, is_channel_1: boolean, is_channel_2: boolean, callback: () => void): void {

        if (ch0.length === 0 || ch1.length === 0) {
            callback();
            return;
        }

        let plotCh0: number[][] = [];
        let plotCh1: number[][] = [];

        if (is_channel_1 && is_channel_2) {
            plotCh0 = ch0;
            plotCh1 = ch1;
            // plotData = [ch0, ch1];
        } else if (is_channel_1 && !is_channel_2) {
            plotCh0 = ch0;
            plotCh1 = [];
        } else if (!is_channel_1 && is_channel_2) {
            plotCh0 = [];
            plotCh1 = ch1;
        } else {
            plotCh0 = [];
            plotCh1 = [];
        }

        const plt_data: jquery.flot.dataSeries[] = [{label: label1, data: plotCh0}, {label: label2, data: plotCh1}];

        if (this.reset_range) {
            this.options.xaxis.min = rangeX.from;
            this.options.xaxis.max = rangeX.to;
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