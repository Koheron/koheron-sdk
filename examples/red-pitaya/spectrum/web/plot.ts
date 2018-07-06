// Plot
// (c) Koheron

class Plot {

    private plot: jquery.flot.plot;
    private options: jquery.flot.plotOptions;
    private isResetRange: boolean;

    private minY: number;
    private maxY: number;
    private rangeY: jquery.flot.range;

    private xLabel: string;
    private yLabel: string;
    private xLabelDiv: HTMLDivElement;

    private velocitySwitch: HTMLInputElement;
    private isPlotVelocity: boolean;
    private velocity: Array<number>;

    constructor(document: Document, private placeholder: JQuery, private driver: Spectrum) {

        this.setPlot();

        this.minY = 0;
        this.maxY = 200;

        this.rangeY = {
            from: this.minY,
            to: this.maxY
        };

        this.yLabel = "Power Spectral Density (dB)";
        this.xLabelDiv = <HTMLDivElement>document.getElementById("x-label");
        this.xLabel = "Frequency (MHz)";
        this.isPlotVelocity = false;
        this.velocity = [];
        this.velocitySwitch = <HTMLInputElement>document.getElementById("velocity-switch");
        this.initVelocitySwitch();
        this.updatePlot();
    }

    updatePlot(): void {

        this.xLabelDiv.innerHTML = this.xLabel;

        var plotData: number[][] = [];
        var plotRangeX: jquery.flot.range = {
            from: 0,
            to: 0
        };

        if (this.isPlotVelocity) {
            this.driver.getPeakFifoData( (peakFifoData) => {

                var time: number[] = [];
                var velocityData: number[][] = [];

                const samplingFrequency: number = 125e6; //Hz
                const fftSize: number = 4096;
                const dopplerShift: number = 1.29e6;
                const n: number = Math.floor(samplingFrequency / fftSize);
                let fifoLength: number = peakFifoData.length;
                this.velocity = this.rollArray(this.velocity, fifoLength);

                for (let i: number = 0; i < fifoLength; i ++) {
                    this.velocity[n-i] = peakFifoData[fifoLength - i] * (samplingFrequency / fftSize) / dopplerShift;
                }

                for (let i : number = n; i > 0 ; i --) {
                    time[i] = (i / samplingFrequency) * fftSize;
                }
                for (let i: number = 0; i < n; i ++) {
                    velocityData[i] = [time[i], this.velocity[i]];
                }

                this.driver.setAddressRange(2, fftSize / 2);
                plotData = velocityData;
                plotRangeX = {
                    from: 0,
                    to: 1
                }

                this.redraw(plotData, plotRangeX, () => {
                    requestAnimationFrame( () => { this.updatePlot(); });
                });

            });
        } else {
            this.driver.getDecimatedData( (decimatedData: number[][], decimatedRangeX: jquery.flot.range) => {

                this.redraw(decimatedData, decimatedRangeX, () => {
                    requestAnimationFrame( () => { this.updatePlot(); });
                });

            });
        }



    }

    // == Plot

    setPlot(): void {
        this.isResetRange = false;

        let labelAttribute: string =  "";
        labelAttribute += " style='font-size: 16px; color: #333'";

        this.options = {
            series: {
                shadowSize: 0 // Drawing is faster without shadows
            },
            yaxis: {
                min: this.minY,
                max: this.maxY
            },
            xaxis: {
                min: 0,
                max: 62.5,
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
            colors: ["#019cd5"],
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
        this.autoScale();
        this.onWheel();
        this.isResetRange = true;
    }

    rangeSelect(): void {
        this.placeholder.bind('plotselected', (event: JQueryEventObject, ranges: jquery.flot.ranges) => {
            // Clamp the zooming to prevent external zoom
            if (ranges.xaxis.to - ranges.xaxis.from < 0.00001) {
                ranges.xaxis.to = ranges.xaxis.from + 0.00001;
            }
            if (ranges.yaxis.to - ranges.yaxis.from < 0.00001) {
                ranges.yaxis.to = ranges.yaxis.from + 0.00001;
            }

            this.rangeY = {
                from: ranges.yaxis.from,
                to: ranges.yaxis.to
            };

            this.driver.setFreqRange(ranges.xaxis);
            this.resetRange();
        });
    }

    // A double click on the plot resets to full span
    autoScale(): void {
        this.placeholder.bind('dblclick', (evt: JQueryEventObject) => {
            const rangeX: jquery.flot.range = {
                from : 0,
                to   : 62.5
            };

            this.rangeY = <jquery.flot.range>{};
            this.driver.setFreqRange(rangeX);
            this.resetRange();
        });
    }

    onWheel(): void {
        this.placeholder.bind('wheel', (evt: JQueryEventObject) => {
            let delta: number = (<JQueryMousewheel.JQueryMousewheelEventObject>evt.originalEvent).deltaX
                              + (<JQueryMousewheel.JQueryMousewheelEventObject>evt.originalEvent).deltaY;
            delta /= Math.abs(delta);

            const zoomRatio: number = 0.2;

            if ((<JQueryInputEventObject>evt.originalEvent).shiftKey) { // Zoom Y
                const positionY: number = (<JQueryMouseEventObject>evt.originalEvent).pageY - this.plot.offset().top;
                const y0: any = this.plot.getAxes().yaxis.c2p(<any>positionY);

                this.rangeY = {
                    from: y0 - (1 + zoomRatio * delta) * (y0 - this.plot.getAxes().yaxis.min),
                    to: y0 - (1 + zoomRatio * delta) * (y0 - this.plot.getAxes().yaxis.max)
                };

                this.resetRange();
                return false;
            } else if ((<JQueryInputEventObject>evt.originalEvent).altKey) { // Zoom X
                const positionX: number = (<JQueryMouseEventObject>evt.originalEvent).pageX - this.plot.offset().left;
                const freq0: any = this.plot.getAxes().xaxis.c2p(<any>positionX);

                if (freq0 < 0 || freq0  > 62.5) {
                    return;
                }

                const rangeX: jquery.flot.range = {
                    from: Math.max(freq0 - (1 + zoomRatio * delta) * (freq0 - this.plot.getAxes().xaxis.min), 0),
                    to: Math.min(freq0 - (1 + zoomRatio * delta) * (freq0 - this.plot.getAxes().xaxis.max), 62.5)
                };

                this.driver.setFreqRange(rangeX);
                this.resetRange();
                return false;
            }

            return true
        });
    }

    resetRange(): void {
        this.isResetRange = true;
    }

    redraw(data: number[][], rangeX: jquery.flot.range, callback: () => void): void {

        const plt_data: jquery.flot.dataSeries[] = [{label: this.yLabel, data: data}];

        if (data.length == 0) {
            callback();
            return;
        }

        if (this.isResetRange) {
            this.options.xaxis.min = rangeX.from;
            this.options.xaxis.max = rangeX.to;
            this.options.yaxis.min = this.rangeY.from;
            this.options.yaxis.max = this.rangeY.to;
            this.plot = $.plot(this.placeholder, plt_data, this.options);
            this.plot.setupGrid();
            this.isResetRange = false;
        } else {
            this.plot.setData(plt_data);
            this.plot.draw();
        }

        callback();
    }

    initVelocitySwitch(): void {
        this.velocitySwitch.addEventListener('change', (event) => {
            if (this.isPlotVelocity) {
                this.yLabel = "Power Spectral Density (dB)";
                this.xLabel = "Frequency (MHz)";
                this.isPlotVelocity = false;
                this.isResetRange = true;
            } else {
                this.yLabel = "Speed (m/s)";
                this.xLabel = "Time (s)";
                this.isPlotVelocity = true;
                this.isResetRange = true;
            }
        })
    }

    rollArray(array, count) {
        // numpy roll
        count -= array.length * Math.floor(count / array.length);
        array.push.apply(array, array.splice(0, count))
        return array
    }

}