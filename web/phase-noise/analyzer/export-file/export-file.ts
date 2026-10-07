// Acquisition metadata and labels for this analyzer.
// (c) Koheron

class ExportFile extends PnaExportFile<IParameters> {
    protected metadata(parameters: IParameters, receivedAt: string): string[] {
        const clock = this.document.body.dataset.board === 'red-pitaya' ? 'Fixed onboard'
            : parameters.clkIndex === '2' ? 'Internal' : 'External';
        return [
            '"Frame received at",' + (receivedAt || ''),
            '"Input channel",' + parameters.channel,
            '"Sampling frequency (Hz)",' + parameters.fs,
            '"Reference clock",' + clock,
            '"LO 0 frequency (Hz)",' + parameters.fdds0,
            '"LO 1 frequency (Hz)",' + parameters.fdds1,
            '"Decimation rate",' + parameters.cic_rate,
            '"Averaging window (spectra)",' + parameters.fft_navg,
            '"Analyzer mode",' + parameters.analyzer_mode,
            '"Interferometer delay (s)",' + parameters.interferometer_delay,
            '',
            '"Offset frequency (Hz)","' + this.plot_.yLabel + '","' + this.plot_.yLabel + ' (smoothed)","Phase PSD (rad^2/Hz)"'
        ];
    }

    protected frameLabel(p: IParameters): string {
        return `ADC ${p.channel} · CIC ${p.cic_rate} · averaging window ${p.fft_navg} · ${p.analyzer_mode.toUpperCase()}`;
    }
}
