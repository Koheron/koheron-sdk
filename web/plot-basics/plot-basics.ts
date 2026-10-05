// Plot widget
// (c) Koheron

class PlotBasics {

    private plotTitleSpan: HTMLSpanElement;

    private range_x: jquery.flot.range;
    private range_y: jquery.flot.range;
    
    private log_x: boolean;
    private log_y: boolean;
    public LogYaxisFormatter;
    private decimate: boolean;
    private spectrumReduction = false;
    private batchedLines = false;
    private drawnWidth: number;
    private drawnHeight: number;

    private reset_range: boolean;
    private options: jquery.flot.plotOptions;
    private plot: jquery.flot.plot;
    private seriesOne: jquery.flot.dataSeries[];

    private isPeakDetection: boolean = true;
    private peakDatapointSpan: HTMLSpanElement;
    private peakDatapoint: number[];

    private hoverDatapointSpan: HTMLSpanElement;
    private hoverDatapoint: number[];

    private clickDatapointSpan: HTMLSpanElement;
    private clickDatapoint: number[];
    private clickSeriesIndex = 0;
    private clickTraceLabel: string;
    private primaryTraceLabel: string;

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

        this.log_x = false;
        this.log_y = false;
        this.decimate = false;

        this.setPlot(this.range_x.from, this.range_x.to, this.range_y.from, this.range_y.to);
        this.seriesOne = [{ label: '', data: [] }];
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

        this.LogYaxisFormatter = (val, axis) => {};

        this.initUnitInputs();
        this.initPeakDetection();
    }

    setPlot(x_min: number, x_max: number, y_min: number, y_max: number) {
        this.reset_range = false;

        this.options = {
            canvas: true,
            series: {
                shadowSize: 0, // Drawing is faster without shadows
                lines: { show: true, lineWidth: 2, fill: false },
                points: { show: false }
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

    refreshLegend() {
        this.reset_range = true;
    }

    setPrimaryTraceLabel(label: string): void {
        this.primaryTraceLabel = label;
        this.refreshLegend();
    }

    setRangeX(from: number, to: number) {
        this.x_min = from;
        this.x_max = to;
        this.range_x.from = from;
        this.range_x.to = to;
        this.reset_range = true;
    }

    setVisibleRangeX(from: number, to: number): void {
        this.range_x.from = from; this.range_x.to = to; this.reset_range = true;
    }

    getRangeX(): {from: number; to: number} {
        return {from: this.range_x.from, to: this.range_x.to};
    }

    private static readonly log10T = (v: number) => Math.log(v) * Math.LOG10E;
    private static readonly pow10  = (v: number) => Math.exp(v * Math.LN10);

    setLogX(adaptiveTicks = false) {
        this.log_x = true;

        this.options.xaxis.transform = PlotBasics.log10T;
        this.options.xaxis.inverseTransform = PlotBasics.pow10;

        // majors only (powers of 10) for labels
        this.options.xaxis.ticks = (axis) => {
            const min = Math.max(axis.min, 1e-300);
            const max = axis.max;
            const pMin = Math.floor(Math.log10(min));
            const pMax = Math.ceil(Math.log10(max));
            const majors: number[] = [];

            for (let p = pMin; p <= pMax; p++) {
                majors.push(Math.pow(10, p));
            }
            if (adaptiveTicks) {
                const visible = majors.filter(v => v >= min && v <= max);
                if (visible.length >= 2 || !(max > min)) { return visible; }
                // A zoom between decade marks still needs frequency labels.
                const intermediate: number[] = [];
                for (let p = pMin; p <= pMax; p++) {
                    for (const m of [1, 2, 5]) {
                        const value = m * Math.pow(10, p);
                        if (value >= min && value <= max) { intermediate.push(value); }
                    }
                }
                if (intermediate.length >= 2) { return intermediate; }
                const rawStep = (max - min) / 4;
                const power = Math.pow(10, Math.floor(Math.log10(rawStep)));
                const fraction = rawStep / power;
                const step = power * (fraction <= 1 ? 1 : fraction <= 2 ? 2 : fraction <= 5 ? 5 : 10);
                const first = Math.ceil(min / step) * step;
                const ticks: number[] = [];
                for (let i = 0; i < 8 && first + i * step <= max; i++) { ticks.push(first + i * step); }
                return ticks;
            }
            return majors;
        };

        this.options.xaxis.tickDecimals = 0;
        this.options.xaxis.tickFormatter = (val: number, axis) => {
            if (adaptiveTicks) {
                const scale = val >= 1e6 ? 1e6 : val >= 1e3 ? 1e3 : 1;
                const digits = Math.min(12, Math.max(0, Math.ceil(-Math.log10((axis.max - axis.min) / scale / 4))));
                return String(Number((val / scale).toFixed(digits))) + (scale === 1e6 ? 'M' : scale === 1e3 ? 'k' : '');
            }
            if (val >= 1e6) {
                return (val / 1e6).toFixed(axis.tickDecimals || 0) + "M";
            }

            if (val >= 1e3) {
                return (val / 1e3).toFixed(axis.tickDecimals || 0) + "k";
            }

            return val.toFixed(axis.tickDecimals || 0);
        };

        // add minor vertical grid lines between decades
        this.options.grid.markings = (axes) => {
            const min = Math.max(axes.xaxis.min, 1e-300);
            const max = axes.xaxis.max;
            const pMin = Math.floor(Math.log10(min));
            const pMax = Math.ceil(Math.log10(max));

            const markings: any[] = [];

            for (let p = pMin; p <= pMax; p++) {
                const base = Math.pow(10, p);

                // 2..9 * 10^p are minors (skip 1*10^p to avoid doubling the major line)
                for (let m = 2; m < 10; m++) {
                    const v = m * base;
                    if (v >= min && v <= max) {
                        markings.push({
                            color: "#eee",      // lighter than your #d5d5d5
                            lineWidth: 1,
                            xaxis: { from: v, to: v },
                        });
                    }
                }
            }

            return markings;
        };
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

    enableDecimation() {
        this.decimate = true;
    }

    needsRedraw(): boolean {
        if (this.batchedLines && this.plot) {
            const width = this.plot_placeholder.width(), height = this.plot_placeholder.height();
            if (width > 0 && height > 0 && (width !== this.drawnWidth || height !== this.drawnHeight)) {
                this.reset_range = true;
            }
        }
        return this.reset_range;
    }

    // Large, jagged paths are expensive for the canvas rasterizer even when
    // issuing their drawing commands is fast. Short overlapping paths retain
    // the same line and joins without tessellating the entire noise trace.
    enableBatchedLines(): void {
        this.batchedLines = true;
        const widths = new Map<any, number>();
        const restoreLines = () => {
            widths.forEach((width, series) => { series.lines.lineWidth = width; });
            widths.clear();
        };
        this.options.hooks = <jquery.flot.hooks>{
            processOptions: [], processRawData: [], processDatapoints: [], processOffset: [],
            drawBackground: [restoreLines], bindEvents: [], drawOverlay: [], shutdown: [restoreLines],
            drawSeries: [(plot, context, series) => {
                if (!series.lines.show || series.lines.fill || series.lines.steps || series.shadowSize > 0 ||
                    !(series.lines.lineWidth > 0) || series.data.length < 256) { return; }
                PlotBasics.drawBatchedLines(context, series, plot.getPlotOffset(), plot.width(), plot.height());
                widths.set(series, series.lines.lineWidth);
                // Flot still owns the series, axes, legend and hit testing.
                // Suppress only its duplicate stroke during this draw call.
                series.lines.lineWidth = 0;
            }],
            draw: [restoreLines]
        };
    }

    private static drawBatchedLines(context: CanvasRenderingContext2D, series: any,
        offset: {left: number; top: number}, width: number, height: number): void {
        context.save();
        context.translate(offset.left, offset.top);
        context.beginPath(); context.rect(0, 0, width, height); context.clip();
        context.strokeStyle = series.color;
        context.lineWidth = series.lines.lineWidth;
        context.lineJoin = 'round';
        let previous: number[], endpoint: number[], lastSegment: number[];
        let segments = 0;
        const flush = () => {
            if (segments) { context.stroke(); }
            context.beginPath(); segments = 0;
        };
        context.beginPath();
        for (const point of series.data) {
            const x = point[0], y = point[1];
            if (x == null || y == null || !Number.isFinite(x) || !Number.isFinite(y)) {
                flush(); previous = endpoint = lastSegment = undefined; continue;
            }
            if (previous) {
                const line = PlotBasics.clipLine(previous, point, series.xaxis, series.yaxis);
                if (line) {
                    const pixel = [series.xaxis.p2c(line[0]), series.yaxis.p2c(line[1]),
                        series.xaxis.p2c(line[2]), series.yaxis.p2c(line[3])];
                    if (pixel.every(Number.isFinite)) {
                        if (segments >= 32) {
                            flush();
                            // Repeat the last segment so its join survives the
                            // batch boundary. Opt-in trace colors are opaque.
                            context.moveTo(lastSegment[0], lastSegment[1]);
                            context.lineTo(lastSegment[2], lastSegment[3]); segments = 1;
                        }
                        if (!endpoint || endpoint[0] !== pixel[0] || endpoint[1] !== pixel[1]) {
                            context.moveTo(pixel[0], pixel[1]);
                        }
                        context.lineTo(pixel[2], pixel[3]); segments++;
                        endpoint = [pixel[2], pixel[3]]; lastSegment = pixel;
                    }
                }
            }
            previous = point;
        }
        flush();
        context.restore();
    }

    // Clip in data coordinates before applying a logarithmic transform, as
    // Flot does. Extreme Y zooms never send enormous coordinates to canvas.
    private static clipLine(from: number[], to: number[], xaxis: any, yaxis: any): number[] {
        const line = [from[0], from[1], to[0], to[1]];
        for (const [index, axis] of [[1, yaxis], [0, xaxis]] as [number, any][]) {
            for (const [bound, lower] of [[axis.min, true], [axis.max, false]] as [number, boolean][]) {
                const outside = (value: number) => lower ? value < bound : value > bound;
                const first = outside(line[index]), last = outside(line[index + 2]);
                if (first && last) { return; }
                if (first !== last) {
                    const t = (bound - line[index]) / (line[index + 2] - line[index]);
                    const other = 1 - index;
                    const intersection = line[other] + t * (line[other + 2] - line[other]);
                    const end = first ? 0 : 2;
                    line[index + end] = bound; line[other + end] = intersection;
                }
            }
        }
        return line;
    }

    disableDecimation() {
        this.decimate = false;
    }

    enableSpectrumReduction() {
        this.decimate = false;
        this.spectrumReduction = true;
    }

    // Keep both extrema per screen column, in frequency order. Retain NaN gaps
    // and the boundary neighbours so zooming does not invent connecting lines.
    static reduceSpectrum(data: number[][], from: number, to: number, width: number): number[][] {
        if (!data.length || !(to > from)) { return data; }
        width = Math.max(1, Math.floor(width));
        let first = 0, last = data.length - 1;
        while (first < last && data[first][0] < from) { first++; }
        first = Math.max(0, first - 1);
        while (last > first && data[last][0] > to) { last--; }
        last = Math.min(data.length - 1, last + 1);
        if (last - first + 1 <= 2 * width) {
            return first === 0 && last === data.length - 1 ? data : data.slice(first, last + 1);
        }
        const out: number[][] = [];
        let column = -Infinity, min = -1, max = -1, previous = -1;
        const push = (index: number) => {
            if (index >= 0 && index !== previous) { out.push(data[index]); previous = index; }
        };
        const flush = () => {
            if (min <= max) { push(min); push(max); }
            else { push(max); push(min); }
            min = max = -1;
        };
        push(first);
        for (let i = first; i <= last; i++) {
            const nextColumn = Math.floor((data[i][0] - from) * width / (to - from));
            if (nextColumn !== column || !Number.isFinite(data[i][1])) {
                flush(); column = nextColumn;
            }
            if (!Number.isFinite(data[i][1])) { push(i); continue; }
            if (min < 0 || data[i][1] < data[min][1]) { min = i; }
            if (max < 0 || data[i][1] > data[max][1]) { max = i; }
        }
        flush(); push(last);
        return out;
    }

    updateDatapointSpan(datapoint: number[], datapointSpan: HTMLSpanElement, prefix = ''): void {
        let positionX: number = (this.plot.pointOffset({x: datapoint[0], y: datapoint[1] })).left;
        let positionY: number = (this.plot.pointOffset({x: datapoint[0], y: datapoint[1] })).top;

        datapointSpan.innerHTML = prefix + "(" + (datapoint[0].toFixed(2)).toString() + "," + datapoint[1].toFixed(2).toString() + ")";

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

    private _decimated: number[][] = [];

    // binary searches on already-sorted data by x
    private bsLeft(data: number[][], x: number) {
        let lo = 0, hi = data.length;
        while (lo < hi) {
            const mid = (lo + hi) >>> 1;
            if (data[mid][0] < x) {
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }
        return Math.min(Math.max(lo, 0), Math.max(data.length - 1, 0));
    }

    private bsRight(data: number[][], x: number) {
        let lo = 0, hi = data.length;
        while (lo < hi) {
            const mid = (lo + hi) >>> 1;
            if (data[mid][0] <= x) {
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }
        return Math.min(Math.max(lo - 1, 0), Math.max(data.length - 1, 0));
    }

    // Decimate visible slice by canvas columns (log-x aware)
    private decimateToCanva(plot_data: number[][], xMin: number, xMax: number): number[][] {
        const out = this._decimated; out.length = 0;

        if (!plot_data.length || !(xMax > xMin)) {
            return out;
        }

        const ph = this.plot?.getPlaceholder() ?? this.plot_placeholder;
        const wAll = ph.width() || 800;
        const off = this.plot ? this.plot.getPlotOffset() : { left: 0, right: 0 };
        const innerW = Math.max(1, Math.floor(wAll - (off.left || 0) - (off.right || 0)));
        // Use the requested range, rather than the previous Flot axes. During
        // startup or zoom, old axes can otherwise discard boundary bins and
        // omit their extrema from the new automatic Y range.
        const transform = this.log_x ? Math.log10 : (x: number) => x;
        const lower = transform(xMin);
        const span = transform(xMax) - lower;
        const colFromX = (x: number) => Math.min(innerW - 1,
            Math.floor(innerW * (transform(x) - lower) / span));

        const i0 = this.bsLeft(plot_data, xMin);
        const i1 = this.bsRight(plot_data, xMax);
        const first = Math.max(0, i0 - 1), last = Math.min(plot_data.length - 1, i1 + 1);
        // Sparse zooms show every bin, including neighbors needed to clip the
        // curve at the edges. Column extrema are only needed for dense traces.
        if (last - first + 1 <= 2 * innerW) {
            return plot_data.slice(first, last + 1);
        }
        if (first < i0) { out.push(plot_data[first]); }
    
        let currCol = -2;
        let minY = Infinity, maxY = -Infinity, minI = -1, maxI = -1;
        const flush = () => {
            // Extrema must retain their original frequency order.
            if (minI >= 0 && maxI >= 0) {
                out.push(plot_data[Math.min(minI, maxI)]);
                if (maxI !== minI) out.push(plot_data[Math.max(minI, maxI)]);
            }
            minY = Infinity; maxY = -Infinity; minI = -1; maxI = -1;
        };

        for (let i = i0; i <= i1; i++) {
            const x = plot_data[i][0];
            const y = plot_data[i][1];
            const col = colFromX(x);
            if (col < 0 || col >= innerW) continue;
            if (col !== currCol) {
                flush();
                currCol = col;
            }
            if (!Number.isFinite(y)) {
                flush();
                // Keep a gap marker even when its neighbors share one pixel.
                if (!out.length || Number.isFinite(out[out.length - 1][1]))
                    out.push([x, NaN]);
                continue;
            }
            if (y < minY) { minY = y; minI = i; }
            if (y > maxY) { maxY = y; maxI = i; }
        }
        flush();
        if (last > i1) { out.push(plot_data[last]); }
        return out;
    }

    redraw(plot_data: number[][], n_pts: number, peakDatapoint: number[], ylabel: string, callback: () => void, reference?: number[][], peakIsFinal = false, traces: jquery.flot.dataSeries[] = [], overlayLabel?: string) {
        this.seriesOne.length = (reference ? 2 : 1) + traces.length;
        this.options.legend.noColumns = overlayLabel || this.primaryTraceLabel ? 0 : reference || traces.length ? 1 : 0;
        if (reference) {
            this.seriesOne[1] = {label: overlayLabel || "Reference", data: reference, color: overlayLabel ? "#006400" : "#a178b5", lines: {lineWidth: 1}};
        }
        traces.forEach((trace, i) => { this.seriesOne[(reference ? 2 : 1) + i] = {...trace, lines: {lineWidth: 1, ...trace.lines}}; });
        if (!this.plot) {
            this.seriesOne[0].label = this.primaryTraceLabel || (!overlayLabel && (reference || traces.length) ? "Live · " + ylabel : ylabel);
            this.seriesOne[0].data  = []; // temporary
            this.plot = $.plot(this.plot_placeholder, this.seriesOne, this.options);
        }

        if (this.spectrumReduction) {
            const offsets = this.plot.getPlotOffset();
            const width = Math.max(1, (this.plot_placeholder.width() || 800) - offsets.left - offsets.right);
            this.seriesOne[0].data = PlotBasics.reduceSpectrum(plot_data, this.range_x.from, this.range_x.to, width);
            if (reference) {
                this.seriesOne[1].data = PlotBasics.reduceSpectrum(reference, this.range_x.from, this.range_x.to, width);
            }
            traces.forEach((trace, i) => {
                this.seriesOne[(reference ? 2 : 1) + i].data = PlotBasics.reduceSpectrum(trace.data as number[][], this.range_x.from, this.range_x.to, width);
            });
        } else if (this.decimate) {
            const xMin = this.reset_range ? this.range_x.from : this.plot.getAxes().xaxis.min;
            const xMax = this.reset_range ? this.range_x.to   : this.plot.getAxes().xaxis.max;
            this.seriesOne[0].data = this.decimateToCanva(plot_data, xMin, xMax).slice();
            if (reference) this.seriesOne[1].data = this.decimateToCanva(reference, xMin, xMax).slice();
            traces.forEach((trace, i) => {
                this.seriesOne[(reference ? 2 : 1) + i].data = this.decimateToCanva(trace.data as number[][], xMin, xMax).slice();
            });
        } else {
            this.seriesOne[0].data  = plot_data;
        }

        this.seriesOne[0].label = this.primaryTraceLabel || (!overlayLabel && (reference || traces.length) ? "Live · " + ylabel : ylabel);

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
            this.plot = $.plot(this.plot_placeholder, this.seriesOne, this.options);
            this.plot.setupGrid();

            this.range_y.from = this.plot.getAxes().yaxis.min;
            this.range_y.to = this.plot.getAxes().yaxis.max;

            this.reset_range = false;
        } else {
            this.plot.setData(this.seriesOne);
            this.plot.draw();
        }

        let localData: jquery.flot.dataSeries[] = this.plot.getData();

        if (this.spectrumReduction || this.batchedLines) { this.plot.unhighlight(); }
        else { setTimeout(() => {this.plot.unhighlight()}, 100); }

        const extra = traces.findIndex(trace => trace.label === this.clickTraceLabel);
        const clickSeries = this.clickTraceLabel ? (reference ? 2 : 1) + extra : this.clickSeriesIndex || 0;
        const cursorData = this.clickTraceLabel ? (extra >= 0 ? traces[extra].data : undefined) : clickSeries === 1 ? reference : plot_data;
        if (!cursorData) {
            this.clickDatapoint = [];
            this.clickDatapointSpan.style.display = "none";
        }
        if (this.clickDatapoint.length > 0 && cursorData && cursorData.length > 0) {
            let i: number;
            for (i = 0; i < cursorData.length; i++) {
                if (cursorData[i][0] > this.clickDatapoint[0]) {
                    break;
                }
            }

            let p1 = cursorData[i-1];
            let p2 = cursorData[i];

            if ((p1 === null) || (p1 === undefined)) {
                this.clickDatapoint[1] = p2[1];
            } else if ((p2 === null) || (p2 === undefined)) {
                this.clickDatapoint[1] = p1[1];
            } else {
                this.clickDatapoint[1] = p1[1] + (p2[1] - p1[1]) * (this.clickDatapoint[0] - p1[0]) / (p2[0] - p1[0]);
            }

            if (  this.range_x.from < this.clickDatapoint[0] && this.clickDatapoint[0] < this.range_x.to
                &&this.range_y.from < this.clickDatapoint[1] && this.clickDatapoint[1] < this.range_y.to) {
                this.updateDatapointSpan(this.clickDatapoint, this.clickDatapointSpan, this.clickTraceLabel ? this.clickTraceLabel + " " : clickSeries === 1 ? "Ref " : "");
                this.clickDatapointSpan.style.display = "inline-block";
                this.plot.highlight(localData[clickSeries], this.clickDatapoint);
            } else {
                this.clickDatapointSpan.style.display = "none";
            }
        }

        if (this.isPeakDetection && peakDatapoint.length > 0) {
            for (let i: number = 0; !peakIsFinal && i < plot_data.length; i++) {
                
                if (peakDatapoint[1] < plot_data[i][1]) {
                    peakDatapoint[0] = plot_data[i][0];
                    peakDatapoint[1] = plot_data[i][1];
                }
            }

            this.plot.unhighlight(localData[0], peakDatapoint);

            if (   this.range_x.from < peakDatapoint[0] && peakDatapoint[0] < this.range_x.to
                && this.range_y.from < peakDatapoint[1] && peakDatapoint[1] < this.range_y.to) {
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

        if (this.batchedLines) {
            this.drawnWidth = this.plot_placeholder.width();
            this.drawnHeight = this.plot_placeholder.height();
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

    redrawTwoChannels(ch0: number[][],
                      ch1: number[][],
                      range_x: jquery.flot.range,
                      label1: string,
                      label2: string,
                      is_channel_1: boolean,
                      is_channel_2: boolean,
                      callback: () => void): void {
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
                    from: Math.max(x0 - (1 + zoomRatio * delta) * (x0 - this.plot.getAxes().xaxis.min), this.log_x ? this.x_min : 0),
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
                this.updateDatapointSpan(this.hoverDatapoint, this.hoverDatapointSpan, item.series.label === "Reference" ? "Ref " : ["Average", "Max hold", "Smoothed"].includes(item.series.label) ? item.series.label + " " : "");
            } else {
                this.hoverDatapointSpan.style.display = "none";
            }
        });
    }

    showClickPoint(): void {
        this.plot_placeholder.bind("plotclick", (event: JQueryEventObject, pos, item) => {
            if (item) {
                this.clickTraceLabel = ["Average", "Max hold", "Smoothed"].includes(item.series.label) ? item.series.label : undefined;
                this.clickSeriesIndex = item.series.label === "Reference" ? 1 : 0;
                this.clickDatapoint[0] = item.datapoint[0];
                this.clickDatapoint[1] = item.datapoint[1];

                this.clickDatapointSpan.style.display = "inline-block";
                this.updateDatapointSpan(this.clickDatapoint, this.clickDatapointSpan, item.series.label === "Reference" ? "Ref " : ["Average", "Max hold", "Smoothed"].includes(item.series.label) ? item.series.label + " " : "");

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
