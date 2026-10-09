// Retain raw phase density (including signed cross spectra), never display dB.
interface PnaReference<P> extends PlotReference {
    psd: Float32Array;
    parameters: P;
    data?: number[][];
    smooth?: number[][];
    negative?: number[][];
    negativeSmooth?: number[][];
    plotType?: string;
}

class PnaReferences<P extends PnaPlotParameters> extends PlotReferences<PnaReference<P>> {
    constructor(board: string, private signed: boolean) { super(board); }

    capture(psd: Float32Array, parameters: P, receivedAt: string): void {
        this.add({psd:psd.slice(), parameters:{...parameters}}, receivedAt);
    }

    replace(item: PnaReference<P>, psd: Float32Array, parameters: P, receivedAt: string): void {
        this.replaceCapture(item, {psd:psd.slice(), parameters:{...parameters}}, receivedAt);
    }

    serialize(): string {
        return JSON.stringify({format:'koheron-phase-noise-references', version:1, board:this.board,
            references:this.items.map(({name, capturedAt, visible, color, psd, parameters}) =>
                ({name, capturedAt, visible, color, psd:Array.from(psd), parameters}))}, null, 2);
    }

    load(text: string): void {
        const items = this.parseFile(text, 'koheron-phase-noise-references');
        const finite = (v: unknown): v is number => typeof v === 'number' && Number.isFinite(v);
        const positive = (v: unknown): v is number => finite(v) && v > 0;
        const integer = (v: unknown): v is number => Number.isSafeInteger(v) && (v as number) >= 0;
        const fail = (): never => { throw new Error('Invalid phase-noise reference data or acquisition settings.'); };
        const parsed = items.map(item => {
            const p = item.parameters;
            if (!Array.isArray(item.psd) || item.psd.length < 3 || item.psd.length > 262145 ||
                !item.psd.every((v: unknown) => v === null || (finite(v) && Math.abs(v) <= 3.4028234663852886e38 && (this.signed || v >= 0))) ||
                !p || p.data_size !== item.psd.length || !positive(p.fs) ||
                !integer(p.channel) || p.channel > (this.signed ? 2 : 1) ||
                !integer(p.cic_rate) || p.cic_rate < 1 || !integer(p.fft_navg) || p.fft_navg < 1 ||
                !finite(p.fdds0) || !finite(p.fdds1) ||
                (p.clkIndex !== undefined && !['0', '2', 'fixed'].includes(p.clkIndex))) { fail(); }
            if (this.signed) {
                if (!finite(p.fdds2) || !finite(p.fdds3) || !integer(p.avgxy_count)) { fail(); }
            } else if (!['rf', 'laser'].includes(p.analyzer_mode) || !positive(p.interferometer_delay)) { fail(); }
            // New firmware includes publication, precision and averaging metadata.
            // Retain these optional scalars without admitting arbitrary object graphs.
            const optionalNumbers = ['sequence', 'state', 'precision', 'average_target', 'avgxy_count', 'min_freq', 'fdds2', 'fdds3', 'interferometer_delay'];
            for (const key of optionalNumbers) {
                if (p[key] !== undefined && !finite(p[key])) { fail(); }
            }
            if (p.analyzer_mode !== undefined && !['rf', 'laser'].includes(p.analyzer_mode)) { fail(); }
            const parameters: any = {};
            for (const key of ['data_size', 'fs', 'channel', 'cic_rate', 'fft_navg', 'fdds0', 'fdds1', 'clkIndex',
                'analyzer_mode', ...optionalNumbers]) {
                if (p[key] !== undefined) { parameters[key] = p[key]; }
            }
            return {name:item.name.trim(), capturedAt:item.capturedAt, visible:item.visible, color:item.color,
                parameters:parameters as P, psd:Float32Array.from(item.psd, (v: number) => v === null ? NaN : v)};
        });
        this.append(parsed);
    }
}
