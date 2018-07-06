// Plot widget
// (c) Koheron

class Plot {
    private n_pts: number;

    private min_y: number = -200;
    private max_y: number = 170;

    private range_x: jquery.flot.range;
    private range_y: jquery.flot.range;

    private reset_range: boolean;
    private options: jquery.flot.plotOptions;
    public plot: jquery.flot.plot;
    public plot_data: Array<Array<number>>;

    public yLabel: string = "Power Spectral Density";

    private isMeasure: boolean = true;

    private isPeakDetection: boolean = true;
    private peakDatapointSpan: HTMLSpanElement;
    private peakDatapoint: number[];

    private hoverDatapointSpan: HTMLSpanElement;
    private hoverDatapoint: number[];

    private clickDatapointSpan: HTMLSpanElement;
    private clickDatapoint: number[];

    constructor(document: Document, private plot_placeholder: JQuery, private fft: FFT) {
        this.setPlot();

        this.range_x = <jquery.flot.range>{};
        this.range_x.from = 0;
        this.range_x.to = this.fft.status.fs / 1E6 / 2;

        this.range_y = <jquery.flot.range>{};
        this.range_y.from = this.min_y;
        this.range_y.to = this.max_y;

        this.n_pts = this.fft.fft_size / 2;

        this.hoverDatapointSpan = <HTMLSpanElement>document.getElementById("hover-datapoint");
        this.hoverDatapoint = [];

        this.clickDatapointSpan = <HTMLSpanElement>document.getElementById("click-datapoint");
        this.clickDatapoint = [];

        this.peakDatapointSpan = <HTMLSpanElement>document.getElementById("peak-datapoint");
        this.peakDatapoint = [];

        this.plot_data = [];

        this.updatePlot();
        this.initUnitInputs();
        this.initPeakDetection();
    }

    updatePlot() {
        this.fft.read_psd( (psd: Float32Array) => {
            this.redraw(psd, () => {
                requestAnimationFrame( () => { this.updatePlot(); } );
            });
        });
    }

    setPlot() {
        this.reset_range = false;

        let labelAttribute: string =  "";
        labelAttribute += " style='font-size: 16px; color: #333'";

        this.options = {
            canvas: true,
            series: {
                shadowSize: 0 // Drawing is faster without shadows
            },
            yaxis: {
                min: this.min_y,
                max: this.max_y
            },
            xaxis: {
                min: 0,
                max: this.fft.status.fs / 1E6 / 2,
                show: true
            },
            grid: {
                margin: {
                    top: 0,
                    left: 0,
                },
                borderColor: "#d5d5d5",
                borderWidth: 1,
                clickable: this.isMeasure,
                hoverable: this.isMeasure,
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
                    return "<b" + labelAttribute + ">" + label + "\t</b>"
                    },
                margin: 0,
                position: "ne",
            }
        }

        this.rangeSelect();
        this.dblClick();
        this.onWheel();
        this.showHoverPoint();
        this.showClickPoint();
        this.plotLeave();
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
            this.range_x.to = this.fft.status.fs / 1E6 / 2;

            this.range_y = <jquery.flot.range>{};
            this.resetRange();
        });
    }

    resetRange() {
        this.reset_range = true;
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

    redraw(psd: Float32Array, callback: () => void) {
        let yUnit: string = (<HTMLInputElement>document.querySelector(".unit-input:checked")).value;
        this.peakDatapoint = [ this.fft.status.fs / 1E6 / 2 / this.n_pts , this.convertValue(psd[0], yUnit)];

        for (let i: number = 0; i <= this.n_pts; i++) {
            let freq: number = (i + 1) * this.fft.status.fs / 1E6 / 2 / this.n_pts; // MHz
            let convertedPsd: number = this.convertValue(psd[i], yUnit);
            this.plot_data[i] = [freq, convertedPsd];

            if (this.peakDatapoint[1] < this.plot_data[i][1]) {
                this.peakDatapoint[0] = this.plot_data[i][0];
                this.peakDatapoint[1] = this.plot_data[i][1];
            }
        }

        const plt_data: jquery.flot.dataSeries[] = [{label: this.yLabel, data: this.plot_data}];

        if (this.reset_range) {
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

        setTimeout(this.plot.unhighlight(), 100);

        if (this.clickDatapoint.length > 0) {
            let i: number;
            for (i = 0; i < this.n_pts; i++) {
                if (localData[0]['data'][i][0] > this.clickDatapoint[0]) {
                    break;
                }
            }

            let p1 = localData[0]['data'][i-1];
            let p2 = localData[0]['data'][i];

            if (p1 === null) {
                this.clickDatapoint[1] = p2[1];
            } else if (p2 === null) {
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

        if (this.isPeakDetection) {
            this.plot.unhighlight(localData[0], this.peakDatapoint);

            if (this.range_x.from < this.peakDatapoint[0] && this.peakDatapoint[0] < this.range_x.to &&
                  this.range_y.from < this.peakDatapoint[1] &&  this.peakDatapoint[1] < this.range_y.to) {
                this.updateDatapointSpan(this.peakDatapoint, this.peakDatapointSpan);
                this.plot.highlight(localData[0], this.peakDatapoint);
                this.peakDatapointSpan.style.display = "inline-block";
            } else {
                this.plot.unhighlight(localData[0], this.peakDatapoint);
                this.peakDatapointSpan.style.display = "none";
            }
        } else {
            this.plot.unhighlight(localData[0], this.peakDatapoint);
            this.peakDatapointSpan.style.display = "none";
        }

        callback();
    }

    onWheel(): void {
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

                this.resetRange();
                return false;
            } else if ((<JQueryInputEventObject>evt.originalEvent).altKey) { // Zoom X
                const positionX: number = (<JQueryMouseEventObject>evt.originalEvent).pageX - this.plot.offset().left;
                const x0: any = this.plot.getAxes().xaxis.c2p(<any>positionX);

                if (x0 < 0 || x0  > this.fft.status.fs / 1E6 / 2) {
                    return;
                }

                this.range_x = {
                    from: Math.max(x0 - (1 + zoomRatio * delta) * (x0 - this.plot.getAxes().xaxis.min), 0),
                    to: Math.min(x0 - (1 + zoomRatio * delta) * (x0 - this.plot.getAxes().xaxis.max), this.fft.status.fs / 1E6 / 2)
                };

                this.resetRange();
                return false;
            }

            return true;
        });
    }

    initUnitInputs(): void {
        let unitInputs: HTMLInputElement[] = <HTMLInputElement[]><any>document.getElementsByClassName("unit-input");
        for (let i = 0; i < unitInputs.length; i ++) {
            unitInputs[i].addEventListener( 'change', (event) => {
                this.resetRange();
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
