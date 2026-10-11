// Flot supplies the same axes, grid, selection and canvas renderer as FFT/PNA.
// This adapter only maps complex transmission data and keeps the two X views in sync.
class VNAPlot {
    markers = [1e6, 10e6, 50e6];
    delayView = false;
    private range: [number, number] | null = null;
    private observer: ResizeObserver;
    private placeholders: HTMLElement[];
    private plots: jquery.flot.plot[] = [];
    private events = new InstrumentEvents();
    private disposed = false;
    private current: VNADisplay | null = null;
    private hoveredFrequency: number | null = null;
    private pendingDraw = false;
    private pixelRatio = 0;
    private viewKeys: string[] = [];
    private yRanges: {context: string; references: VNAFrame[]; calibration: VNAFrame | null; min: number; max: number}[] = [];
    constructor(private model: VNAModel, private document: Document, private changed: () => void) {
        this.placeholders = ['magnitude-plot', 'phase-plot'].map(id => document.getElementById(id));
        this.observer = new ResizeObserver(() => { if (!this.disposed) this.draw(); });
        for (const host of this.placeholders) {
            this.observer.observe(host);
            const element = $(host);
            let selected = false;
            this.events.listen(host, 'pointerdown', () => { selected = false; });
            element.on('plotselected.vna', (_event, ranges) => {
                const low = ranges.xaxis.from, high = ranges.xaxis.to;
                if (this.model.latest && low > 0 && high > low) { selected = true; this.range = [low, high]; this.draw(); }
            });
            element.on('plotunselected.vna', () => { if (this.pendingDraw) this.draw(); });
            element.on('plothover.vna', (_event, position) => {
                const axes = this.plots[this.placeholders.indexOf(host)]?.getAxes();
                this.hoveredFrequency = axes && position.x >= axes.xaxis.min && position.x <= axes.xaxis.max &&
                    position.y >= axes.yaxis.min && position.y <= axes.yaxis.max ? position.x : null;
                this.renderHover();
            });
            this.events.listen(host, 'pointerleave', () => { this.hoveredFrequency = null; this.renderHover(); });
            element.on('plotclick.vna', (_event, position) => {
                if (selected || !this.model.latest || !(position.x > 0)) return;
                this.markers[0] = this.model.latest.frequency[this.nearest(this.model.latest.frequency, position.x)];
                this.changed(); this.draw();
            });
            this.events.listen(host, 'dblclick', () => this.reset());
            this.events.listen(host, 'keydown', (event: KeyboardEvent) => { if (event.key === 'Escape') this.reset(); });
        }
    }
    dispose(): void {
        this.disposed = true; this.observer.disconnect(); this.events.dispose();
        this.plots.forEach(plot => plot.shutdown());
        this.placeholders.forEach(host => $(host).off('.vna'));
    }
    reset(): void { this.range = null; this.yRanges = []; this.draw(); }
    nearest(frequency: number[], hz: number): number {
        if (!(hz > 0)) return 0;
        let low = 0, high = frequency.length - 1;
        while (high - low > 1) { const middle = (low + high) >> 1; if (frequency[middle] < hz) low = middle; else high = middle; }
        return Math.abs(Math.log(frequency[low] / hz)) <= Math.abs(Math.log(frequency[high] / hz)) ? low : high;
    }
    private number(v: number, unit: string): string { return Number.isFinite(v) ? `${v.toFixed(2)} ${unit}` : '—'; }
    private renderHover(): void {
        const current = this.current;
        const readout = this.document.getElementById('hover-reading');
        if (!current || this.hoveredFrequency === null) { readout.textContent = 'Drag to zoom · double-click to reset'; return; }
        const i = this.nearest(current.frame.frequency, this.hoveredFrequency);
        const value = this.delayView ? this.number(current.delay[i], 'ns') : this.number(current.phase[i], '°');
        readout.textContent = `${vnaFrequency(current.frame.frequency[i])} · ${this.number(current.magnitude[i], 'dB')} · ${value}`;
    }
    private bounds(): [number, number] {
        const s = this.model.latest?.settings || this.model.settings;
        return this.range || [s.start, s.stop];
    }
    private values(display: VNADisplay, panel: number): number[] {
        return panel === 0 ? display.magnitude : this.delayView ? display.delay : display.phase;
    }
    private verticalBounds(panel: number, finite: number[], low: number, high: number,
        current: VNADisplay | null, references: VNAFrame[]): [number, number] {
        if (panel && !this.delayView && !this.model.unwrap) {
            delete this.yRanges[panel]; return [-180, 180];
        }
        const mode = panel === 0 ? 'magnitude' : this.delayView ? 'delay' : 'phase';
        const s = current?.frame.settings;
        const settings = s && [s.start, s.stop, s.points, s.duration, s.level, current.frame.recordSamples ?? VNAModel.SAMPLES];
        const context = JSON.stringify([low, high, mode, settings,
            current?.calibrated, panel ? this.model.delayNs : 0]);
        const calibration = current?.calibrated ? this.model.calibration : null;
        const saved = this.yRanges[panel];
        const previous = saved && saved.context === context && saved.calibration === calibration &&
            saved.references.length === references.length && saved.references.every((frame, i) => frame === references[i]) ? saved : null;
        if (!previous) delete this.yRanges[panel];
        if (!finite.length) {
            return previous ? [previous.min, previous.max] : panel === 0 ? [-80, 10] : this.delayView ? [0, 100] : [-180, 180];
        }
        const dataMin = Math.min(...finite), dataMax = Math.max(...finite);
        const padding = Math.max(panel === 0 ? 3 : 1, (dataMax - dataMin) * .12);
        // Measurement noise must not move a grid that still contains the trace.
        // Expand only the crossed edge; Reset or a changed view fits it anew.
        const min = previous && dataMin >= previous.min ? previous.min : dataMin - padding;
        const max = previous && dataMax <= previous.max ? previous.max : dataMax + padding;
        this.yRanges[panel] = {context, references, calibration, min, max};
        return [min, max];
    }
    draw(): void {
        if (this.disposed) return;
        // A sweep finishing during selection must not advance just one panel.
        // Flot's selected/unselected event resumes both from the newest frame.
        if (this.plots.some(plot => plot.isSelectionActive?.())) { this.pendingDraw = true; return; }
        this.pendingDraw = false;
        const current = this.current = this.model.latest ? this.model.display(this.model.latest) : null;
        const pixelRatio = this.document.defaultView.devicePixelRatio || 1;
        const visible = this.model.references.filter(t => t.visible);
        const references = visible.map(t => t.frame);
        const traces = visible.map(t => ({...this.model.display(t.frame, false), color: t.color, name: t.name}));
        if (current) traces.unshift({...current, color: '#019cd5', name: 'Live'});
        this.placeholders.forEach((host, panel) => {
            if (host.clientWidth < 1 || host.clientHeight < 1) return;
            const [low, high] = this.bounds();
            const finite = traces.flatMap(t => this.values(t, panel).filter((v, i) => Number.isFinite(v) && t.frame.frequency[i] >= low && t.frame.frequency[i] <= high));
            const [min, max] = this.verticalBounds(panel, finite, low, high, current, references);
            const ticks: number[] = [];
            for (let decade = Math.floor(Math.log10(low)); decade <= Math.ceil(Math.log10(high)); ++decade) {
                for (const multiple of [1, 2, 5]) {
                    const hz = multiple * 10 ** decade;
                    if (hz >= low && hz <= high) ticks.push(hz);
                }
            }
            if (ticks.length < 2) ticks.push(low, high);
            const series: jquery.flot.dataSeries[] = traces.map(trace => {
                const data: (number | null)[][] = [], values = this.values(trace, panel);
                values.forEach((value, i) => {
                    // Never draw through invalid bins or a wrapped phase jump.
                    if (panel && !this.delayView && !this.model.unwrap && i > 0 && Math.abs(value - values[i - 1]) > 180)
                        data.push([trace.frame.frequency[i], null]);
                    data.push([trace.frame.frequency[i], Number.isFinite(value) ? value : null]);
                });
                return {data, color: trace.color, label: PlotReferences.traceLabel(trace.name)};
            });
            const options: jquery.flot.plotOptions = {
                canvas: true,
                series: {shadowSize: 0, lines: {show: true, lineWidth: 1.5}},
                xaxis: {min: low, max: high, transform: Math.log, inverseTransform: Math.exp,
                    ticks, tickFormatter: hz => hz !== low && hz !== high && ticks.length > 9 && !/^1(?:\.0+)?e/.test(hz.toExponential()) ? '' : vnaFrequency(hz)},
                yaxis: {min, max, labelWidth: 45},
                grid: {borderColor: '#d5d5d5', borderWidth: 1, hoverable: true, clickable: true, autoHighlight: false},
                selection: {mode: 'x', touch: false},
                legend: {show: traces.length > 1, position: 'ne', backgroundOpacity: .85},
                hooks: {drawOverlay: [(plot, context) => this.drawMarkers(plot, context, this.current, panel)]}
            };
            const plot = this.plots[panel];
            const size = host.getBoundingClientRect();
            const viewKey = JSON.stringify([low, high, min, max, size.width, size.height, series.map(s => [s.label, s.color])]);
            if (plot && plot.updateRanges && this.pixelRatio === pixelRatio) {
                if (this.viewKeys[panel] === viewKey) {
                    // Like FFT, redraw data directly when axes and legend stay
                    // fixed. Keep the existing grid, text caches and canvases.
                    plot.setData(series); plot.draw();
                } else {
                    Object.assign(plot.getAxes().xaxis.options, options.xaxis);
                    plot.getOptions().legend.show = traces.length > 1;
                    plot.updateRanges(series, options.xaxis, options.yaxis, 1);
                }
            } else {
                plot?.shutdown();
                this.plots[panel] = $.plot($(host), series, options);
            }
            this.viewKeys[panel] = viewKey;
            // Flot initializes its highlight array after the first draw. Make
            // overlays synchronous once construction finishes, including PNGs.
            this.plots[panel].getOptions().interaction.redrawOverlayInterval = -1;
            this.plots[panel].triggerRedrawOverlay();
        });
        this.pixelRatio = pixelRatio;
        this.renderHover();
        this.document.getElementById('empty-state').hidden = !!current;
    }
    private drawMarkers(plot: jquery.flot.plot, ctx: CanvasRenderingContext2D, current: VNADisplay | null, panel: number): void {
        if (!current) return;
        const [low, high] = this.bounds(), offset = plot.getPlotOffset(), axes = plot.getAxes();
        ctx.save(); ctx.translate(offset.left, offset.top);
        ctx.beginPath(); ctx.rect(0, 0, plot.width(), plot.height()); ctx.clip();
        this.markers.forEach((hz, marker) => {
            if (hz < low || hz > high) return;
            const i = this.nearest(current.frame.frequency, hz), value = this.values(current, panel)[i];
            const x = Number(axes.xaxis.p2c(hz as any));
            ctx.strokeStyle = '#888'; ctx.lineWidth = 1; ctx.setLineDash([3, 4]);
            ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, plot.height()); ctx.stroke(); ctx.setLineDash([]);
            if (Number.isFinite(value)) {
                ctx.fillStyle = 'white'; ctx.strokeStyle = '#019cd5'; ctx.lineWidth = 1.5;
                ctx.beginPath(); ctx.arc(Number(axes.xaxis.p2c(current.frame.frequency[i] as any)), Number(axes.yaxis.p2c(value as any)), 3, 0, 2 * Math.PI); ctx.fill(); ctx.stroke();
            }
            ctx.fillStyle = '#666'; ctx.font = '10px Lato, Arial, sans-serif'; ctx.textAlign = 'center'; ctx.fillText(`M${marker + 1}`, Math.max(12, Math.min(plot.width() - 12, x)), 12);
        });
        ctx.restore();
    }
    image(): HTMLCanvasElement {
        const result = this.document.createElement('canvas'), scale = 2;
        const width = Math.max(800, this.placeholders[0].clientWidth);
        const heights = this.placeholders.map(host => host.clientHeight);
        const ctx = result.getContext('2d');
        const display = this.model.display(this.model.latest), frame = this.model.latest;
        ctx.font = '11px Lato, Arial, sans-serif';
        const metadata = this.wrapText(ctx, `${frame.source.toUpperCase()} · ${frame.timestamp} · ${display.calibrated ? 'Thru corrected' : 'Uncalibrated'} · ${frame.settings.level} dBFS · extension ${this.model.delayNs} ns`, width - 24);
        const references = this.wrapText(ctx, 'Live trace: blue · References: ' + (this.model.references.filter(t => t.visible).map(t => t.name).join(', ') || 'none'), width - 24);
        const header = 64 + metadata.length * 15, footer = 24 + references.length * 15;
        result.width = width * scale; result.height = (header + heights[0] + heights[1] + 30 + footer) * scale;
        ctx.scale(scale, scale); ctx.fillStyle = 'white'; ctx.fillRect(0, 0, result.width, result.height);
        ctx.fillStyle = '#333'; ctx.font = 'bold 16px Lato, Arial, sans-serif'; ctx.fillText(`${VNAProfile.label} · Forward transmission`, 12, 30);
        ctx.font = '11px Lato, Arial, sans-serif'; ctx.fillStyle = '#666';
        metadata.forEach((line, i) => ctx.fillText(line, 12, 50 + 15 * i));
        for (let panel = 0; panel < 2; ++panel) {
            const top = panel === 0 ? header : header + heights[0] + 30;
            ctx.fillText(panel === 0 ? 'Magnitude (dB)' : this.delayView ? 'Group delay (ns)' : 'Phase (degrees)', 12, top - 8);
            this.placeholders[panel].querySelectorAll<HTMLCanvasElement>('canvas').forEach(canvas => ctx.drawImage(canvas, 0, top, width, heights[panel]));
        }
        references.forEach((line, i) => ctx.fillText(line, 12, header + heights[0] + heights[1] + 50 + 15 * i));
        return result;
    }
    private wrapText(ctx: CanvasRenderingContext2D, text: string, width: number): string[] {
        const lines: string[] = []; let line = '';
        for (const word of text.split(/\s+/)) {
            const next = line ? `${line} ${word}` : word;
            if (line && ctx.measureText(next).width > width) { lines.push(line); line = word; }
            else line = next;
        }
        if (line) lines.push(line);
        return lines;
    }
}
