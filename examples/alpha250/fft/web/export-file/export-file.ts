// (c) Koheron

class ExportFile {
    constructor(private document: Document, private spectrum: Plot) {
        document.querySelector('.export-data').addEventListener('click', () => this.exportData());
        document.querySelector('.export-plot').addEventListener('click', () => this.exportPlot());
    }

    private exportPlot(): void {
        const canvas = this.document.querySelector<HTMLCanvasElement>('#plot-placeholder canvas.flot-base');
        const status = this.spectrum.frameStatus;
        if (!canvas || !status) { return; }
        // Flot's backing canvas can be larger than its CSS size on HiDPI screens.
        // Draw annotations in CSS pixels while retaining every plot image pixel.
        const width = canvas.clientWidth || canvas.width;
        const scale = canvas.width / width;
        const height = canvas.height / scale;
        const image = this.document.createElement('canvas');
        image.width = canvas.width;
        image.height = Math.ceil((height + 82) * scale);
        const context = image.getContext('2d');
        if (!context) { return; }
        context.scale(scale, scale);
        context.fillStyle = 'white';
        context.fillRect(0, 0, width, height + 82);
        context.fillStyle = '#333';
        context.font = '12px sans-serif';
        context.fillText('ALPHA250 FFT · ' + this.spectrum.yLabel, 12, 20);
        const windows = ['Rectangular', 'Hann', 'Flat top', 'Blackman–Harris'];
        context.font = '11px sans-serif';
        context.fillText('ADC ' + status.channel + ' · ' + (windows[status.window_index] || 'Window ' + status.window_index)
            + ' · ' + status.fs / 1e6 + ' MS/s', 12, 38);
        context.drawImage(canvas, 0, 52, width, height);
        context.textAlign = 'center';
        context.fillText('Frequency (MHz)', width / 2, height + 72);
        image.toBlob(blob => { if (blob) { this.download(blob, 'koheron_fft.png'); } });
    }

    private download(blob: Blob, filename: string): void {
        const url = URL.createObjectURL(blob);
        const link = this.document.createElement('a');
        link.href = url;
        link.download = filename;
        link.click();
        window.setTimeout(() => URL.revokeObjectURL(url), 1000);
    }

    private exportData(): void {
        const status = this.spectrum.frameStatus;
        if (!status) { return; }
        // Use the displayed frame's metadata, including while the plot is paused.
        const rows = [
            'Koheron ALPHA250 FFT',
            'Exported at,' + new Date().toISOString(),
            'Window index,' + status.window_index,
            'Input channel,' + status.channel,
            'Sampling frequency (Hz),' + status.fs,
            'Reference clock,' + (status.clkIndex === '0' ? 'External' : 'Internal'),
            'DDS 0 (Hz),' + status.dds_freq[0],
            'DDS 1 (Hz),' + status.dds_freq[1],
            '',
            'Frequency (MHz),' + this.spectrum.yLabel
        ];
        for (const row of this.spectrum.plot_data) { rows.push(row.join(',')); }
        this.download(new Blob([rows.join('\n')], {type: 'text/csv;charset=utf-8'}), 'koheron_fft.csv');
    }
}
