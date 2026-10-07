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
            '"LO 2 frequency (Hz)",' + parameters.fdds2,
            '"LO 3 frequency (Hz)",' + parameters.fdds3,
            '"Decimation rate",' + parameters.cic_rate,
            '"Averaging window (spectra)",' + parameters.fft_navg,
            '"Cumulative XY segments",' + parameters.avgxy_count,
            '',
            '"Offset frequency (Hz)","' + this.plot_.yLabel + '","' + this.plot_.yLabel + ' (smoothed)","Signed phase PSD (rad^2/Hz)"'
        ];
    }

    protected frameLabel(p: IParameters): string {
        return `${["X", "Y", "XY"][p.channel]} · CIC ${p.cic_rate} · ${p.channel === 2 ? p.avgxy_count + " cumulative segments" : "averaging window " + p.fft_navg}`;
    }
}
