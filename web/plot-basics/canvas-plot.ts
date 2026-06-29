// Canvas plot helper for PlotBasics
// (c) Koheron

interface CanvasPlotRange {
    from: number;
    to: number;
}

interface CanvasPlotOptions {
    xaxis: CanvasPlotRange;
    yaxis: CanvasPlotRange;
    logX: boolean;
    logY: boolean;
    ylabel: string;
    yTickFormatter?: (val: number, axis: any) => string;
}

class CanvasPlot {
    public readonly canvas: HTMLCanvasElement;

    private ctx: CanvasRenderingContext2D;
    private data: number[][] = [];
    private markerData: number[][] = [];
    private options: CanvasPlotOptions | null = null;
    private framePending: boolean = false;
    private dpr: number = 1;
    private plotOffset = { left: 58, right: 16, top: 12, bottom: 32 };
    private plotWidth: number = 1;
    private plotHeight: number = 1;
    private cssWidth: number = 1;
    private cssHeight: number = 1;

    constructor(private placeholder: JQuery) {
        this.canvas = document.createElement("canvas");
        this.canvas.style.width = "100%";
        this.canvas.style.height = "100%";
        this.canvas.style.display = "block";
        (<any>this.placeholder).empty().append(this.canvas);

        const ctx = this.canvas.getContext("2d");
        if (!ctx) {
            throw new Error("Canvas 2D context is not available");
        }
        this.ctx = ctx;
    }

    setOptions(options: CanvasPlotOptions): void {
        this.options = options;
    }

    setData(data: number[][]): void {
        this.data = data;
        this.requestDraw();
    }

    setMarkers(markerData: number[][]): void {
        this.markerData = markerData;
        this.requestDraw();
    }

    setDataAndMarkers(data: number[][], markerData: number[][]): void {
        this.data = data;
        this.markerData = markerData;
        this.requestDraw();
    }

    requestDraw(): void {
        if (this.framePending) { return; }
        this.framePending = true;
        requestAnimationFrame(() => {
            this.framePending = false;
            this.draw();
        });
    }

    resize(): void {
        this.dpr = window.devicePixelRatio || 1;
        this.cssWidth = Math.max(1, (<any>this.placeholder).width() || this.canvas.clientWidth || 800);
        this.cssHeight = Math.max(1, (<any>this.placeholder).height() || this.canvas.clientHeight || 300);
        const width = Math.round(this.cssWidth * this.dpr);
        const height = Math.round(this.cssHeight * this.dpr);

        if (this.canvas.width !== width || this.canvas.height !== height) {
            this.canvas.width = width;
            this.canvas.height = height;
        }

        this.ctx.setTransform(this.dpr, 0, 0, this.dpr, 0, 0);
        this.plotWidth = Math.max(1, this.cssWidth - this.plotOffset.left - this.plotOffset.right);
        this.plotHeight = Math.max(1, this.cssHeight - this.plotOffset.top - this.plotOffset.bottom);
    }

    dataToCanvasX(x: number): number {
        const min = this.xTransform(this.options!.xaxis.from);
        const max = this.xTransform(this.options!.xaxis.to);
        return this.plotOffset.left + (this.xTransform(x) - min) * this.plotWidth / this.safeDenominator(max - min);
    }

    dataToCanvasY(y: number): number {
        const min = this.yTransform(this.options!.yaxis.from);
        const max = this.yTransform(this.options!.yaxis.to);
        return this.plotOffset.top + (max - this.yTransform(y)) * this.plotHeight / this.safeDenominator(max - min);
    }

    canvasToDataX(x: number): number {
        const min = this.xTransform(this.options!.xaxis.from);
        const max = this.xTransform(this.options!.xaxis.to);
        return this.xInverse(min + (x - this.plotOffset.left) * (max - min) / this.safeDenominator(this.plotWidth));
    }

    canvasToDataY(y: number): number {
        const min = this.yTransform(this.options!.yaxis.from);
        const max = this.yTransform(this.options!.yaxis.to);
        return this.yInverse(max - (y - this.plotOffset.top) * (max - min) / this.safeDenominator(this.plotHeight));
    }

    canvasColumnForX(x: number): number {
        return Math.floor(this.dataToCanvasX(x) - this.plotOffset.left);
    }

    getInnerWidth(): number {
        this.resize();
        return this.plotWidth;
    }

    isInPlot(x: number, y: number): boolean {
        this.resize();
        return x >= this.plotOffset.left && x <= this.plotOffset.left + this.plotWidth
            && y >= this.plotOffset.top && y <= this.plotOffset.top + this.plotHeight;
    }

    private draw(): void {
        if (!this.options) { return; }
        this.resize();
        this.ctx.clearRect(0, 0, this.cssWidth, this.cssHeight);
        this.drawGrid();
        this.drawLine();
        this.drawMarkers();
    }

    private drawGrid(): void {
        const ctx = this.ctx;
        ctx.save();
        ctx.font = "12px sans-serif";
        ctx.strokeStyle = "#d5d5d5";
        ctx.fillStyle = "#333";
        ctx.lineWidth = 1;

        for (const x of this.ticks(this.options!.xaxis.from, this.options!.xaxis.to, this.options!.logX)) {
            const px = this.dataToCanvasX(x);
            ctx.beginPath();
            ctx.moveTo(px, this.plotOffset.top);
            ctx.lineTo(px, this.plotOffset.top + this.plotHeight);
            ctx.stroke();
            ctx.fillText(this.formatTick(x), px - 12, this.cssHeight - 8);
        }

        for (const y of this.ticks(this.options!.yaxis.from, this.options!.yaxis.to, this.options!.logY)) {
            const py = this.dataToCanvasY(y);
            ctx.beginPath();
            ctx.moveTo(this.plotOffset.left, py);
            ctx.lineTo(this.plotOffset.left + this.plotWidth, py);
            ctx.stroke();
            const label = this.options!.logY && this.options!.yTickFormatter ? this.options!.yTickFormatter(y, { tickDecimals: 0 }) : this.formatTick(y);
            ctx.fillText(label, 4, py + 4);
        }

        ctx.strokeStyle = "#999";
        ctx.strokeRect(this.plotOffset.left, this.plotOffset.top, this.plotWidth, this.plotHeight);
        ctx.restore();
    }

    private drawLine(): void {
        const ctx = this.ctx;
        ctx.save();
        ctx.beginPath();
        ctx.strokeStyle = "#019cd5";
        ctx.lineWidth = 2;
        let started = false;
        for (const point of this.data) {
            const x = point[0];
            const y = point[1];
            if (!this.isDrawable(x, y)) { started = false; continue; }
            const px = this.dataToCanvasX(x);
            const py = this.dataToCanvasY(y);
            if (!started) { ctx.moveTo(px, py); started = true; } else { ctx.lineTo(px, py); }
        }
        ctx.stroke();
        ctx.restore();
    }

    private drawMarkers(): void {
        const ctx = this.ctx;
        ctx.save();
        ctx.fillStyle = "#006400";
        ctx.strokeStyle = "#006400";
        for (const point of this.markerData) {
            if (!this.isDrawable(point[0], point[1])) { continue; }
            const x = this.dataToCanvasX(point[0]);
            const y = this.dataToCanvasY(point[1]);
            ctx.beginPath();
            ctx.arc(x, y, 4, 0, 2 * Math.PI);
            ctx.fill();
        }
        ctx.restore();
    }

    private ticks(min: number, max: number, log: boolean): number[] {
        if (!(max > min)) { return []; }
        if (log) {
            if (max <= 0) { return []; }
            const ticks: number[] = [];
            const pMin = Math.ceil(Math.log(Math.max(min, 1e-300)) * Math.LOG10E);
            const pMax = Math.floor(Math.log(max) * Math.LOG10E);
            for (let p = pMin; p <= pMax; p++) { ticks.push(Math.pow(10, p)); }
            return ticks;
        }
        const ticks: number[] = [];
        const step = (max - min) / 5;
        for (let i = 0; i <= 5; i++) { ticks.push(min + i * step); }
        return ticks;
    }

    private safeDenominator(v: number): number {
        return Math.abs(v) > 1e-300 ? v : 1;
    }

    private formatTick(v: number): string {
        if (Math.abs(v) >= 1e6) { return (v / 1e6).toFixed(0) + "M"; }
        if (Math.abs(v) >= 1e3) { return (v / 1e3).toFixed(0) + "k"; }
        return Math.abs(v) >= 10 ? v.toFixed(0) : v.toFixed(2);
    }

    private isDrawable(x: number, y: number): boolean {
        return Number.isFinite(x) && Number.isFinite(y)
            && (!this.options!.logX || x > 0) && (!this.options!.logY || y > 0);
    }

    private xTransform(v: number): number { return this.options!.logX ? Math.log(Math.max(v, 1e-300)) * Math.LOG10E : v; }
    private yTransform(v: number): number { return this.options!.logY ? Math.log(Math.max(v, 1e-300)) * Math.LOG10E : v; }
    private xInverse(v: number): number { return this.options!.logX ? Math.exp(v * Math.LN10) : v; }
    private yInverse(v: number): number { return this.options!.logY ? Math.exp(v * Math.LN10) : v; }
}
