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
        if (this.plot_.referenceParameters) {
            rows.push('', 'Reference trace', ...this.metadata(this.plot_.referenceParameters, this.plot_.referenceReceivedAt),
                ...this.rows(this.plot_.reference_data, this.plot_.reference_smooth_data, this.plot_.referencePSD));
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
        const wrap = (text: string): string[] => {
            const lines: string[] = [];
            let line = '';
            for (const word of text.split(' ')) {
                const next = line ? line + ' ' + word : word;
                if (line && context.measureText(next).width > width - 24) { lines.push(line); line = word; }
                else { line = next; }
            }
            if (line) { lines.push(line); }
            return lines;
        };
        const labels = [...wrap(this.plot_.yLabel + ' · ' + (this.document.title.split(' · ')[1] || 'Koheron')),
            ...wrap('Live · ' + this.frameLabel(frame))];
        if (this.plot_.frameReceivedAt) { labels.push(...wrap('Received ' + this.plot_.frameReceivedAt)); }
        if (this.plot_.referenceParameters) { labels.push(...wrap('Reference · ' + this.frameLabel(this.plot_.referenceParameters))); }
        const series = this.plot_.plotBasics.plot.getData();
        const legend = series.filter(s => s.label).map(s => ({label: s.label, color: s.color}));
        const headerHeight = 16 + labels.length * 18 + 24;
        image.width = canvas.width;
        image.height = Math.ceil((height + headerHeight + 30) * scale);
        context.scale(scale, scale);
        context.fillStyle = 'white';
        context.fillRect(0, 0, width, height + headerHeight + 30);
        context.fillStyle = '#333';
        context.font = '12px sans-serif';
        labels.forEach((label, i) => context.fillText(label, 12, 20 + i * 18));
        let x = 12;
        const legendY = headerHeight - 16;
        for (const trace of legend) {
            context.fillStyle = trace.color;
            context.fillRect(x, legendY, 10, 10);
            context.fillStyle = '#333';
            context.fillText(trace.label, x + 16, legendY + 10);
            x += 32 + context.measureText(trace.label).width;
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
