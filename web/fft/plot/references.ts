// Captures retain raw power and their own acquisition metadata for unit changes.
interface FFTReference extends PlotReference {
    fftSize: number;
    psd: Float32Array;
    status: IFFTStatus;
    data?: number[][];
    unit?: string;
}

class FFTReferences extends PlotReferences<FFTReference> {
    capture(psd: Float32Array, status: IFFTStatus, fftSize: number): void {
        this.add({fftSize, psd:psd.slice(), status:JSON.parse(JSON.stringify(status))});
    }

    replace(item: FFTReference, psd: Float32Array, status: IFFTStatus, fftSize: number): void {
        this.replaceCapture(item, {fftSize, psd:psd.slice(), status:JSON.parse(JSON.stringify(status))});
    }

    serialize(): string {
        return JSON.stringify({format: 'koheron-fft-references', version: 1, board: this.board,
            references: this.items.map(({data, unit, psd, ...item}) => ({...item, psd: Array.from(psd)}))}, null, 2);
    }

    // Validate the whole file before appending anything to the current session.
    load(text: string): void {
        const items = this.parseFile(text, 'koheron-fft-references');
        const fail = (): never => { throw new Error('Invalid reference file. Use a file saved by this FFT interface.'); };
        const finite = (v: unknown): v is number => typeof v === 'number' && Number.isFinite(v);
        const positive = (v: unknown): v is number => finite(v) && v > 0;
        const parsed: FFTReference[] = items.map((item: any) => {
            if (!Number.isInteger(item.fftSize) || item.fftSize < 2 || item.fftSize > 524288 ||
                !Array.isArray(item.psd) || !item.psd.length || item.psd.length > 262144 ||
                !item.psd.every((v: unknown) => v === null || (finite(v) && v >= 0 && v <= 3.4028234663852886e38))) { fail(); }
            const s = item.status;
            if (!s || !positive(s.fs) || !positive(s.W1) || !positive(s.W2) ||
                !Number.isInteger(s.channel) || s.channel < 0 || s.channel > 3 ||
                !Number.isInteger(s.window_index) || s.window_index < 0 || s.window_index > 3 ||
                !['0', '2', 'fixed'].includes(s.clkIndex) || !Array.isArray(s.dds_freq) ||
                s.dds_freq.length > 2 || !s.dds_freq.every((v: unknown) => finite(v) && v >= 0) ||
                (s.inputRanges !== undefined && (!Array.isArray(s.inputRanges) || s.inputRanges.length > 4 || !s.inputRanges.every(positive))) ||
                (s.acquisitionKey !== undefined && typeof s.acquisitionKey !== 'string')) { fail(); }
            const grid = s.spectrum;
            if (grid) {
                if (grid.unit !== 'Hz' || typeof grid.logarithmic !== 'boolean' ||
                    !Array.isArray(grid.frequencies) || grid.frequencies.length !== item.psd.length ||
                    !grid.frequencies.every((v: unknown, i: number) => finite(v) && v >= 0 && (i === 0 || v > grid.frequencies[i - 1])) ||
                    !Array.isArray(grid.bandwidths) || grid.bandwidths.length !== item.psd.length || !grid.bandwidths.every(positive) ||
                    (grid.binSpacings !== undefined && (!Array.isArray(grid.binSpacings) || grid.binSpacings.length > 16 || !grid.binSpacings.every(positive)))) { fail(); }
            } else if (item.psd.length !== item.fftSize / 2) { fail(); }
            // Voltage and power spectra have different physical units.
            if (!!grid !== (this.board === 'alpha15')) { throw new Error('The reference spectrum units do not match this board.'); }
            const status: IFFTStatus = {fs:s.fs, channel:s.channel, W1:s.W1, W2:s.W2,
                window_index:s.window_index, clkIndex:s.clkIndex, dds_freq:s.dds_freq,
                ...(grid ? {spectrum:grid} : {}), ...(s.inputRanges ? {inputRanges:s.inputRanges} : {}),
                ...(s.acquisitionKey !== undefined ? {acquisitionKey:s.acquisitionKey} : {})};
            return {name:item.name.trim(), capturedAt:item.capturedAt, visible:item.visible, color:item.color,
                fftSize:item.fftSize, psd:Float32Array.from(item.psd, (v: number) => v === null ? NaN : v), status};
        });
        this.append(parsed);
    }
}

class FFTReferencePanel extends PlotReferencePanel<FFTReference> {
    constructor(document: Document, references: FFTReferences, redraw: () => void, recapture: (item: FFTReference) => void) {
        super(document, references, redraw, recapture, item => {
            const windows = ['Rectangular', 'Hann', 'Flat top', 'Blackman–Harris'];
            const channel = item.status.spectrum && item.status.channel >= 2 ? (item.status.channel === 2 ? 'ADC 0 − 1' : 'ADC 0 + 1') : 'ADC ' + item.status.channel;
            const text = channel + ' · ' + windows[item.status.window_index] + ' · ' + item.status.fs / 1e6 + ' MS/s';
            return {text, title:text + ' · ' + item.fftSize + ' point FFT · ' + (item.status.clkIndex === '0' ? 'External' : item.status.clkIndex === 'fixed' ? 'Fixed' : 'Internal') + ' clock'};
        });
    }
}
