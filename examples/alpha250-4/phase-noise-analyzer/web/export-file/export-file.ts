// Export the displayed spectrum and its acquisition metadata.
// (c) Koheron

class ExportFile {
    constructor(private document: Document, private plot_) {
        document.querySelectorAll<HTMLButtonElement>('.export-data').forEach(button => {
            button.disabled = !plot_.frameStatus;
            button.addEventListener('click', () => this.exportData());
        });
        document.querySelectorAll<HTMLButtonElement>('.export-plot').forEach(button => {
            button.disabled = !plot_.frameStatus;
            button.addEventListener('click', () => this.exportPlot());
        });
    }

    private metadata(parameters: IParameters, receivedAt: string): string[] {
        const clock = this.document.body.dataset.board === 'red-pitaya' ? 'Fixed onboard'
            : parameters.clkIndex === '2' ? 'Internal' : 'External';
        return [
            '"Frame received at",' + (receivedAt || ''),
            '"Input channel",' + parameters.channel,
            '"Sampling frequency (Hz)",' + parameters.fs,
            '"Reference clock",' + clock,
            '"LO 0 frequency (Hz)",' + parameters.fdds0,
            '"LO 1 frequency (Hz)",' + parameters.fdds1,
            '"LO 2 frequency (Hz)",' + parameters.fdds2,
            '"LO 3 frequency (Hz)",' + parameters.fdds3,
            '"Decimation rate",' + parameters.cic_rate,
            '"Averaging window (spectra)",' + parameters.fft_navg,
            '"Cumulative XY segments",' + parameters.avgxy_count,
            '',
            '"Offset frequency (Hz)","' + this.plot_.yLabel + '","' + this.plot_.yLabel + ' (smoothed)","Signed phase PSD (rad^2/Hz)"'
        ];
    }

    private rows(data: number[][], smoothed: number[][], psd: Float32Array): string[] {
        const value = (number: number) => Number.isFinite(number) ? String(number) : '';
        return data.map((row, i) => [row[0], row[1], smoothed[i]?.[1], psd[i]].map(value).join(','));
    }

    private exportData(): void {
        const frame: IParameters = this.plot_.frameStatus;
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

    private frameLabel(p: IParameters): string {
        return `${["X", "Y", "XY"][p.channel]} · CIC ${p.cic_rate} · ${p.channel === 2 ? p.avgxy_count + " cumulative segments" : "averaging window " + p.fft_navg}`;
    }

    private exportPlot(): void {
        const frame: IParameters = this.plot_.frameStatus;
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
