// (c) Koheron

class ExportFile {
    constructor(document: Document, private spectrum: Plot) {
        document.querySelector('.export-data').addEventListener('click', () => this.exportData());
        document.querySelector('.export-plot').addEventListener('click', () => {
            const canvas = document.querySelector<HTMLCanvasElement>('#plot-placeholder canvas.flot-base');
            if (!canvas) { return; }
            const image = document.createElement('canvas');
            image.width = canvas.width;
            image.height = canvas.height + 72;
            const context = image.getContext('2d');
            context.fillStyle = 'white';
            context.fillRect(0, 0, image.width, image.height);
            context.fillStyle = '#333';
            context.font = '14px sans-serif';
            context.fillText('Koheron ALPHA250 · ' + this.spectrum.yLabel, 12, 24);
            context.drawImage(canvas, 0, 38);
            context.textAlign = 'center';
            context.fillText('Frequency (MHz)', image.width / 2, image.height - 10);
            image.toBlob(blob => { if (blob) { this.download(blob, 'koheron_fft.png'); } });
        });
    }

    private download(blob: Blob, filename: string): void {
        const url = URL.createObjectURL(blob);
        const link = document.createElement('a');
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
