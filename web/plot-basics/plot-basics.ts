// Plot widget
// (c) Koheron

class PlotBasics {

    private plotTitleSpan: HTMLSpanElement;

    private range_x: jquery.flot.range;
    private range_y: jquery.flot.range;
    
    private log_y: boolean;
    public LogYaxisFormatter;

    private reset_range: boolean;
    private options: jquery.flot.plotOptions;
    private plot: jquery.flot.plot;
    private plot_data: Array<Array<number>>;

    private isPeakDetection: boolean = true;
    private peakDatapointSpan: HTMLSpanElement;
    private peakDatapoint: number[];

    private hoverDatapointSpan: HTMLSpanElement;
    private hoverDatapoint: number[];

    private clickDatapointSpan: HTMLSpanElement;
    private clickDatapoint: number[];

    constructor(document: Document, private plot_placeholder: JQuery, private n_pts: number, public x_min, public x_max, public y_min, public y_max,
        private driver, private rangeFunction, private plotTitle: string) {

        this.plotTitleSpan = <HTMLSpanElement>document.getElementById("plot-title");
        this.plotTitleSpan.textContent =  this.plotTitle;

        this.range_x = <jquery.flot.range>{};
        this.range_x.from = this.x_min;
        this.range_x.to = this.x_max;
        this.range_y = <jquery.flot.range>{};
        this.range_y.from = this.y_min;
        this.range_y.to = this.y_max;
        
        this.log_y = false;

        this.setPlot(this.range_x.from, this.range_x.to, this.range_y.from, this.range_y.to);
        this.rangeSelect(this.rangeFunction);
        this.dblClick(this.rangeFunction);
        this.onWheel(this.rangeFunction);
        this.showHoverPoint();
        this.showClickPoint();
        this.plotLeave();
        this.reset_range = true;

        this.hoverDatapointSpan = <HTMLSpanElement>document.getElementById("hover-datapoint");
        this.hoverDatapoint = [];

        this.clickDatapointSpan = <HTMLSpanElement>document.getElementById("click-datapoint");
        this.clickDatapoint = [];

        this.peakDatapointSpan = <HTMLSpanElement>document.getElementById("peak-datapoint");

        this.plot_data = [];
        this.LogYaxisFormatter = (val, axis) => {};

        this.initUnitInputs();
        this.initPeakDetection();
    }

    setPlot(x_min: number, x_max: number, y_min: number, y_max: number) {
        this.reset_range = false;

        this.options = {
            canvas: true,
            series: {
                shadowSize: 0 // Drawing is faster without shadows
            },
            yaxis: {
                min: y_min,
                max: y_max
            },
            xaxis: {
                min: x_min,
                max: x_max,
                show: true
            },
            grid: {
                margin: {
                    top: 0,
                    left: 0,
                },
                borderColor: "#d5d5d5",
                borderWidth: 1,
                clickable: true,
                hoverable: true,
                autoHighlight: true
            },
            selection: {
                mode: "xy"
            },
            colors: ["#019cd5", "#006400"],
            legend: {
                show: true,
                noColumns: 0,
                labelFormatter: (label: string, series: any): string => {
                    return "<b style='font-size: 16px; color: #333'>" + label + "\t</b>"
                    },
                margin: 0,
                position: "ne",
            }
        }

    }

    rangeSelect(rangeFunction: string) {
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

            if (rangeFunction.length > 0) {
                this.driver[rangeFunction](ranges.xaxis);
            }

            this.reset_range = true;
        });
    }

    // A double click on the plot resets to full span
    dblClick(rangeFunction: string) {
        this.plot_placeholder.bind("dblclick", (evt: JQueryEventObject) => {
            this.range_x.from = this.x_min;
            this.range_x.to = this.x_max;
            this.range_y = <jquery.flot.range>{};
            if (rangeFunction.length > 0) {
                this.driver[rangeFunction](this.range_x);
            }
            this.reset_range = true;
        });
    }

    setRangeX(from: number, to: number) {
        this.range_x.from = from;
        this.range_x.to = to;
        this.reset_range = true;
    }

    setLogX() {
        this.options.xaxis.ticks = [0.001, 0.01, 0.1 ,1 ,10 ,100, 1000, 10000, 100000, 1000000, 10000000];
        this.options.xaxis.tickDecimals = 0;
        this.options.xaxis.transform = (v) => {return v > 0 ? Math.log10(v) : null};
        this.options.xaxis.inverseTransform = (v) => {return v!= null ? Math.pow(10, v) : 0.0};
        this.options.xaxis.tickFormatter = (val, axis) => {
            if (val >= 1E6) {
                return (val / 1E6).toFixed(axis.tickDecimals) + "M";
            } else if (val >= 1E3) {
                return (val / 1E3).toFixed(axis.tickDecimals) + "k";
            } else {
                return val.toFixed(axis.tickDecimals);
            }
        }
    }

    setLogY() {
        this.log_y = true;
        this.range_y = <jquery.flot.range>{};
        this.reset_range = true;
    }
    
    setLinY() {
        this.log_y = false;
        this.range_y = <jquery.flot.range>{};
        this.reset_range = true;
    }

    updateDatapointSpan(datapoint: number[], datapointSpan: HTMLSpanElement): void {
        let positionX: number = (this.plot.pointOffset({x: datapoint[0], y: datapoint[1] })).left;
        let positionY: number = (this.plot.pointOffset({x: datapoint[0], y: datapoint[1] })).top;

        datapointSpan.innerHTML = "(" + (datapoint[0].toFixed(2)).toString() + "," + datapoint[1].toFixed(2).toString() + ")";

        if (datapoint[0] < (this.range_x.from + this.range_x.to) / 2) {
            datapointSpan.style.left = (positionX + 5).toString() + "px";
        } else {
            datapointSpan.style.left = (positionX - 140).toString() + "px";
        }

        if (datapoint[1] < ( (this.range_y.from + this.range_y.to) / 2 ) ) {
            datapointSpan.style.top = (positionY - 50).toString() + "px";
        } else {
            datapointSpan.style.top = (positionY + 5).toString() + "px";
        }
    }

    redraw(plot_data: number[][], n_pts: number, peakDatapoint: number[], ylabel: string, callback: () => void) {
        const plt_data: jquery.flot.dataSeries[] = [{label: ylabel, data: plot_data}];

        if (this.reset_range) {
            if (this.log_y) {
                // /!\ Cannot set ticks lower than 1 /!\
                this.options.yaxis.ticks = [1 ,10 ,100, 1E3, 1E4, 1E5, 1E6, 1E7, 1E8, 1E9];
                this.options.yaxis.tickDecimals = 0;
                this.options.yaxis.transform = (v) => {return v > 0 ? Math.log10(v) : null};
                this.options.yaxis.inverseTransform = (v) => {return v!= null ? Math.pow(10, v) : 0.0};
                this.options.yaxis.tickFormatter = this.LogYaxisFormatter;
            } else {
                this.options.yaxis = {
                    min: this.range_y.from,
                    max: this.range_y.to
                };
            }

            this.options.xaxis.min = this.range_x.from;
            this.options.xaxis.max = this.range_x.to;
            this.options.yaxis.min = this.range_y.from;
            this.options.yaxis.max = this.range_y.to;
            this.plot = $.plot(this.plot_placeholder, plt_data, this.options);
            this.plot.setupGrid();

            this.range_y.from = this.plot.getAxes().yaxis.min;
            this.range_y.to = this.plot.getAxes().yaxis.max;

            this.reset_range = false;
        } else {
            this.plot.setData(plt_data);
            this.plot.draw();
        }

        let localData: jquery.flot.dataSeries[] = this.plot.getData();

        setTimeout(() => {this.plot.unhighlight()}, 100);

        if (this.clickDatapoint.length > 0) {
            let i: number;
            for (i = 0; i < n_pts; i++) {
                if (localData[0]['data'][i][0] > this.clickDatapoint[0]) {
                    break;
                }
            }

            let p1 = localData[0]['data'][i-1];
            let p2 = localData[0]['data'][i];

            if ((p1 === null) || (p1 === undefined)) {
                this.clickDatapoint[1] = p2[1];
            } else if ((p2 === null) || (p2 === undefined)) {
                this.clickDatapoint[1] = p1[1];
            } else {
                this.clickDatapoint[1] = p1[1] + (p2[1] - p1[1]) * (this.clickDatapoint[0] - p1[0]) / (p2[0] - p1[0]);
            }

            if (this.range_x.from < this.clickDatapoint[0] && this.clickDatapoint[0] < this.range_x.to &&
                 this.range_y.from < this.clickDatapoint[1] &&  this.clickDatapoint[1] < this.range_y.to) {
                this.updateDatapointSpan(this.clickDatapoint, this.clickDatapointSpan);
                this.clickDatapointSpan.style.display = "inline-block";
                this.plot.highlight(localData[0], this.clickDatapoint);
            } else {
                this.clickDatapointSpan.style.display = "none";
            }
        }

        if (this.isPeakDetection && peakDatapoint.length > 0) {
            for (let i: number = 0; i <= n_pts; i++) {
                if (peakDatapoint[1] < plot_data[i][1]) {
                    peakDatapoint[0] = plot_data[i][0];
                    peakDatapoint[1] = plot_data[i][1];
                }
            }

            this.plot.unhighlight(localData[0], peakDatapoint);

            if (this.range_x.from < peakDatapoint[0] && peakDatapoint[0] < this.range_x.to &&
            this.range_y.from < peakDatapoint[1] &&  peakDatapoint[1] < this.range_y.to) {
                this.updateDatapointSpan(peakDatapoint, this.peakDatapointSpan);
                this.plot.highlight(localData[0], peakDatapoint);
                this.peakDatapointSpan.style.display = "inline-block";
            } else {
                this.plot.unhighlight(localData[0], peakDatapoint);
                this.peakDatapointSpan.style.display = "none";
            }
        } else {
            this.plot.unhighlight(localData[0], peakDatapoint);
            this.peakDatapointSpan.style.display = "none";
        }

        callback();
    }

    redrawRange(data: number[][], range_x: jquery.flot.range, ylabel: string, callback: () => void): void {
        const plt_data: jquery.flot.dataSeries[] = [{label: ylabel, data: data}];

        if (data.length == 0) {
            callback();
            return;
        }

        if (this.reset_range) {
            this.options.xaxis.min = range_x.from;
            this.options.xaxis.max = range_x.to;
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


    redrawTwoChannels(ch0: number[][], ch1: number[][],
        range_x: jquery.flot.range, label1: string, label2: string, is_channel_1: boolean, is_channel_2: boolean, callback: () => void): void {

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
         this.options.xaxis.min = range_x.from;
         this.options.xaxis.max = range_x.to;
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

    onWheel(rangeFunction: string): void {
        this.plot_placeholder.bind("wheel", (evt: JQueryEventObject) => {
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

                this.reset_range = true;
                return false;
            } else if ((<JQueryInputEventObject>evt.originalEvent).altKey) { // Zoom X
                const positionX: number = (<JQueryMouseEventObject>evt.originalEvent).pageX - this.plot.offset().left;
                const x0: any = this.plot.getAxes().xaxis.c2p(<any>positionX);

                if (x0 < 0 || x0  > this.x_max) {
                    return;
                }

                this.range_x = {
                    from: Math.max(x0 - (1 + zoomRatio * delta) * (x0 - this.plot.getAxes().xaxis.min), 0),
                    to: Math.min(x0 - (1 + zoomRatio * delta) * (x0 - this.plot.getAxes().xaxis.max), this.x_max)
                };

                if (rangeFunction.length > 0) {
                    this.driver[rangeFunction](this.range_x);
                }

                this.reset_range = true;
                return false;
            }

            return true;
        });
    }

    initUnitInputs(): void {
        let unitInputs: HTMLInputElement[] = <HTMLInputElement[]><any>document.getElementsByClassName("unit-input");
        for (let i = 0; i < unitInputs.length; i ++) {
            unitInputs[i].addEventListener( 'change', (event) => {
                this.reset_range = true;
            })
        }
    }

    initPeakDetection(): void {
        let peakInputs: HTMLInputElement[] = <HTMLInputElement[]><any>document.getElementsByClassName("peak-input");
        for (let i = 0; i < peakInputs.length; i ++) {
            peakInputs[i].addEventListener( 'change', (event) => {
                if (this.isPeakDetection) {
                    this.isPeakDetection = false;
                } else {
                    this.isPeakDetection = true;
                }
            })
        }
    }

    showHoverPoint(): void {
        this.plot_placeholder.bind("plothover", (event: JQueryEventObject, pos, item) => {
            if (item) {
                this.hoverDatapoint[0] = item.datapoint[0];
                this.hoverDatapoint[1] = item.datapoint[1];

                this.hoverDatapointSpan.style.display = "inline-block";
                this.updateDatapointSpan(this.hoverDatapoint, this.hoverDatapointSpan);
            } else {
                this.hoverDatapointSpan.style.display = "none";
            }
        });
    }

    showClickPoint(): void {
        this.plot_placeholder.bind("plotclick", (event: JQueryEventObject, pos, item) => {
            if (item) {
                this.clickDatapoint[0] = item.datapoint[0];
                this.clickDatapoint[1] = item.datapoint[1];

                this.clickDatapointSpan.style.display = "inline-block";
                this.updateDatapointSpan(this.clickDatapoint, this.clickDatapointSpan);

                this.plot.unhighlight();
                this.plot.highlight(item.series, this.clickDatapoint);
            }
        });
    }

    plotLeave(): void {
        this.plot_placeholder.bind("mouseleave", (event: JQueryEventObject, pos, item) => {
            this.hoverDatapointSpan.style.display = "none";
        });
    }

}
