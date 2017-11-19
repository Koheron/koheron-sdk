// Plot
// (c) Koheron

class Plot {

    private plot: jquery.flot.plot;
    private options: jquery.flot.plotOptions;
    private isZoomX: boolean = false;
    private isZoomY: boolean = false;
    private zoomXBtn: HTMLLinkElement;
    private zoomYBtn: HTMLLinkElement;
    private isResetRange: boolean;

    private minY: number;
    private maxY: number;
    private rangeY: jquery.flot.range;

    constructor(document: Document, private placeholder: JQuery, private driver: Spectrum) {

        this.zoomXBtn = <HTMLLinkElement>document.getElementById('zoom-x-btn');
        this.zoomYBtn = <HTMLLinkElement>document.getElementById('zoom-y-btn');

        this.setPlot();

        this.minY = 0;
        this.maxY = 200;

        this.rangeY = {
            from: this.minY,
            to: this.maxY
        };

        this.updatePlot();
    }

    updatePlot(): void {
        this.driver.getDecimatedData( (data: number[][], rangeX: jquery.flot.range) => {
            this.redraw(data, rangeX, () => {
                requestAnimationFrame( () => { this.updatePlot(); });
            });
        });
    }

    // == Plot

    setPlot(): void {
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
                min: 0,
                max: 62.5,
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
            colors: ['#0022FF']
        }

        this.rangeSelect();
        this.dblClick();
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
    dblClick(): void {
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

    autoScale(): void {
        const rangeX = {
            from : 0,
            to   : 62.5
        };

        this.rangeY = <jquery.flot.range>{};
        this.driver.setFreqRange(rangeX);
        this.resetRange();
    }

    onWheel(): void {
        this.placeholder.bind('wheel', (evt: JQueryEventObject) => {
            let delta: number = (<JQueryMousewheel.JQueryMousewheelEventObject>evt.originalEvent).deltaX
                              + (<JQueryMousewheel.JQueryMousewheelEventObject>evt.originalEvent).deltaY;
            delta /= Math.abs(delta);

            const zoomRatio: number = 0.2;

            if ((<JQueryInputEventObject>evt.originalEvent).shiftKey || this.isZoomY) { // Zoom Y
                const positionY: number = (<JQueryMouseEventObject>evt.originalEvent).pageY - this.plot.offset().top;
                const y0: any = this.plot.getAxes().yaxis.c2p(<any>positionY);

                this.rangeY = {
                    from: y0 - (1 + zoomRatio * delta) * (y0 - this.plot.getAxes().yaxis.min),
                    to: y0 - (1 + zoomRatio * delta) * (y0 - this.plot.getAxes().yaxis.max)
                };

                this.resetRange();
                return false;
            } else if ((<JQueryInputEventObject>evt.originalEvent).altKey || this.isZoomX) { // Zoom X
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

    zoomX(): void {
        if (!this.isZoomX) {
            this.isZoomX = true;
            if (this.isZoomY) {
                this.isZoomY = false;
                this.zoomYBtn.className = 'btn btn-primary-reversed';
            }
            this.zoomXBtn.className = 'btn btn-primary-reversed active';
        } else {
            this.isZoomX = false;
            this.zoomXBtn.className = 'btn btn-primary-reversed';
        }
    }

    zoomY(): void {
        if (!this.isZoomY) {
            this.isZoomY = true;
            if (this.isZoomX) {
                this.isZoomX = false;
                this.zoomXBtn.className = 'btn btn-primary-reversed';
            }
            this.zoomYBtn.className = 'btn btn-primary-reversed active';
        } else {
            this.isZoomY = false;
            this.zoomYBtn.className = 'btn btn-primary-reversed';
        }
    }

    resetRange(): void {
        this.isResetRange = true;
    }

    redraw(data: number[][], rangeX: jquery.flot.range, callback: () => void): void {

        if (data.length == 0) {
            callback();
            return;
        }

        if (this.isResetRange) {
            this.options.xaxis.min = rangeX.from;
            this.options.xaxis.max = rangeX.to;
            this.options.yaxis.min = this.rangeY.from;
            this.options.yaxis.max = this.rangeY.to;
            this.plot = $.plot(this.placeholder, [data], this.options);
            this.plot.setupGrid();
            this.isResetRange = false;
        } else {
            this.plot.setData([data]);
            this.plot.draw();
        }

        callback();
    }



}