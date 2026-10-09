// Export the displayed spectrum and its acquisition metadata.
// (c) Koheron

abstract class PnaExportFile<Parameters> {
    constructor(protected document: Document, protected plot_) {
        document.querySelectorAll<HTMLButtonElement>('.export-data').forEach(button => {
            button.disabled = !plot_.frameStatus;
            button.addEventListener('click', () => this.exportData());
        });
        document.querySelectorAll<HTMLButtonElement>('.export-plot').forEach(button => {
            button.disabled = !plot_.frameStatus;
            button.addEventListener('click', () => this.exportPlot());
        });
    }

    protected abstract metadata(parameters: Parameters, receivedAt: string): string[];
    protected abstract frameLabel(parameters: Parameters): string;

    private rows(data: number[][], smoothed: number[][], psd: Float32Array): string[] {
        const value = (number: number) => Number.isFinite(number) ? String(number) : '';
        return data.map((row, i) => [row[0], row[1], smoothed[i]?.[1], psd[i]].map(value).join(','));
    }

    private exportData(): void {
        const frame: Parameters = this.plot_.frameStatus;
        if (!frame) { return; }
        const rows = [this.document.title, '"Exported at",' + new Date().toISOString(), '',
            ...this.metadata(frame, this.plot_.frameReceivedAt),
            ...this.rows(this.plot_.plot_data, this.plot_.smooth_plot_data, this.plot_.phase_psd)];
        const quote = (value: string) => '"' + value.replace(/"/g, '""') + '"';
        for (const item of this.plot_.visibleReferences) {
            rows.push('', 'Reference trace', '"Name",' + quote(item.name), '"Color",' + quote(item.color),
                ...this.metadata(item.parameters, item.capturedAt), ...this.rows(item.data, item.smooth, item.psd));
        }
        this.download(new Blob([rows.join('\n') + '\n'], {type: 'text/csv;charset=utf-8'}), 'csv');
    }

    private exportPlot(): void {
        const frame: Parameters = this.plot_.frameStatus;
        const canvas = this.document.querySelector<HTMLCanvasElement>('#plot-placeholder canvas.flot-base');
        if (!frame || !canvas) { return; }
        // Keep every backing-canvas pixel, including on HiDPI displays.
        const width = canvas.clientWidth || canvas.width;
        const scale = canvas.width / width;
        const height = canvas.height / scale;
        const image = this.document.createElement('canvas');
        const context = image.getContext('2d');
        if (!context) { return; }
        context.font = '12px sans-serif';
        const wrap = (text: string, available = width - 24): string[] => {
            const lines: string[] = [];
            let line = '';
            for (const word of text.split(' ')) {
                const next = line ? line + ' ' + word : word;
                if (context.measureText(next).width <= available) { line = next; continue; }
                if (line) { lines.push(line); line = ''; }
                for (const char of word) {
                    if (line && context.measureText(line + char).width > available) { lines.push(line); line = ''; }
                    line += char;
                }
            }
            if (line) { lines.push(line); }
            return lines;
        };
        const labels = [...wrap(this.plot_.yLabel + ' · ' + (this.document.title.split(' · ')[1] || 'Koheron')),
            ...wrap('Live · ' + this.frameLabel(frame))];
        if (this.plot_.frameReceivedAt) { labels.push(...wrap('Received ' + this.plot_.frameReceivedAt)); }
        for (const item of this.plot_.visibleReferences) {
            labels.push(...wrap(item.name + ' · ' + this.frameLabel(item.parameters)), ...wrap('Captured ' + item.capturedAt));
        }
        const series = this.plot_.plotBasics.plot.getData();
        const legend = series.filter(s => s.label).map(s => {
            // Flot labels contain escaped HTML; PNG uses the original text.
            const text = this.document.createElement('textarea'); text.innerHTML = s.label;
            return {label:text.value, color:s.color};
        });
        const legendRows: {label:string; color:string; x:number; row:number}[] = [];
        let x = 12, row = 0;
        for (const trace of legend) {
            for (const label of wrap(trace.label, width - 56)) {
                const advance = 32 + context.measureText(label).width;
                if (x > 12 && x + advance > width - 12) { row++; x = 12; }
                legendRows.push({label, color:trace.color, x, row}); x += advance;
            }
        }
        const headerHeight = 16 + labels.length * 18 + (row + 1) * 24;
        image.width = canvas.width;
        image.height = Math.ceil((height + headerHeight + 30) * scale);
        context.scale(scale, scale);
        context.fillStyle = 'white';
        context.fillRect(0, 0, width, height + headerHeight + 30);
        context.fillStyle = '#333';
        context.font = '12px sans-serif';
        labels.forEach((label, i) => context.fillText(label, 12, 20 + i * 18));
        for (const trace of legendRows) {
            const y = 16 + labels.length * 18 + trace.row * 24 + 8;
            context.fillStyle = trace.color;
            context.fillRect(trace.x, y, 10, 10);
            context.fillStyle = '#333';
            context.fillText(trace.label, trace.x + 16, y + 10);
        }
        context.drawImage(canvas, 0, headerHeight, width, height);
        context.textAlign = 'center';
        context.fillText('Offset frequency (Hz)', width / 2, height + headerHeight + 20);
        image.toBlob(blob => { if (blob) { this.download(blob, 'png'); } }, 'image/png');
    }

    private download(blob: Blob, extension: string): void {
        const url = URL.createObjectURL(blob);
        const link = this.document.createElement('a');
        const board = (this.document.body.dataset.board || 'analyzer').replace(/[^a-z0-9-]/g, '');
        link.href = url;
        link.download = `phase-noise-${board}-${new Date().toISOString().replace(/[:.]/g, '-')}.${extension}`;
        link.click();
        window.setTimeout(() => URL.revokeObjectURL(url), 1000);
    }
}
