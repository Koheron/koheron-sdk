// Captures retain raw power and their own acquisition metadata for unit changes.
interface FFTReference {
    name: string;
    capturedAt: string;
    visible: boolean;
    color: string;
    fftSize: number;
    psd: Float32Array;
    status: IFFTStatus;
    data?: number[][];
    unit?: string;
}

class FFTReferences {
    static readonly limit = 8;
    private colors = ['#a178b5', '#d55e00', '#0072b2', '#009e73', '#cc79a7', '#827000', '#555555', '#7a3e00'];
    items: FFTReference[] = [];
    private undoItems: FFTReference[];
    private replacedIndex = -1;
    undoLabel = '';
    onChange: () => void = () => {};
    constructor(readonly board: string) {}

    capture(psd: Float32Array, status: IFFTStatus, fftSize: number): void {
        if (this.items.length >= FFTReferences.limit) { throw new Error('Keep up to 8 references. Remove one before capturing another.'); }
        const used = new Set(this.items.map(item => item.color));
        let number = 1;
        while (this.items.some(item => item.name === 'Reference ' + number)) { number++; }
        this.forgetUndo();
        this.items.push({name: 'Reference ' + number, capturedAt: new Date().toISOString(), visible: true,
            color: this.colors.find(color => !used.has(color)) || this.colors[0], fftSize,
            psd: psd.slice(), status: JSON.parse(JSON.stringify(status))});
        this.onChange();
    }

    private forgetUndo(): void { this.undoItems = undefined; this.undoLabel = ''; this.replacedIndex = -1; }

    remove(item: FFTReference): void {
        if (!this.items.includes(item)) { return; }
        this.replacedIndex = -1;
        this.undoItems = this.items.slice(); this.undoLabel = 'Removed ' + item.name;
        this.items = this.items.filter(reference => reference !== item); this.onChange();
    }

    replace(item: FFTReference, psd: Float32Array, status: IFFTStatus, fftSize: number): void {
        const index = this.items.indexOf(item); if (index < 0) { return; }
        this.replacedIndex = index;
        this.undoItems = this.items.slice(); this.undoLabel = 'Recaptured ' + item.name;
        this.items[index] = {...item, capturedAt:new Date().toISOString(), fftSize,
            psd:psd.slice(), status:JSON.parse(JSON.stringify(status)), data:undefined, unit:undefined};
        this.onChange();
    }

    clear(): void {
        if (!this.items.length) { return; }
        this.replacedIndex = -1;
        this.undoItems = this.items.slice(); this.undoLabel = 'Cleared ' + this.items.length + ' references';
        this.items = []; this.onChange();
    }

    undo(): void {
        if (!this.undoItems) { return; }
        if (this.replacedIndex >= 0) {
            // Undo acquisition changes without discarding later naming/visibility edits.
            const current = this.items[this.replacedIndex], previous = this.undoItems[this.replacedIndex];
            previous.name = current.name; previous.visible = current.visible;
        }
        this.items = this.undoItems; this.forgetUndo(); this.onChange();
    }

    serialize(): string {
        return JSON.stringify({format: 'koheron-fft-references', version: 1, board: this.board,
            references: this.items.map(({data, unit, psd, ...item}) => ({...item, psd: Array.from(psd)}))}, null, 2);
    }

    // Validate the whole file before appending anything to the current session.
    load(text: string): void {
        let file: any;
        try { file = JSON.parse(text); } catch (_) { throw new Error('Could not read this file. Choose a saved reference JSON file.'); }
        const fail = (): never => { throw new Error('Invalid reference file. Use a file saved by this FFT interface.'); };
        if (!file || file.format !== 'koheron-fft-references' || file.version !== 1) { fail(); }
        if (file.board !== this.board) { throw new Error('This reference file is for a different board.'); }
        if (!Array.isArray(file.references) || !file.references.length || file.references.length > FFTReferences.limit) { fail(); }
        if (file.references.length + this.items.length > FFTReferences.limit) {
            throw new Error('Loading this file would exceed 8 references. Remove some references first.');
        }
        const finite = (v: unknown): v is number => typeof v === 'number' && Number.isFinite(v);
        const positive = (v: unknown): v is number => finite(v) && v > 0;
        const parsed: FFTReference[] = file.references.map((item: any) => {
            if (!item || typeof item.name !== 'string' || !item.name.trim() || item.name.length > 80 ||
                typeof item.capturedAt !== 'string' || !Number.isFinite(Date.parse(item.capturedAt)) ||
                typeof item.visible !== 'boolean' || typeof item.color !== 'string' || !/^#[0-9a-f]{6}$/i.test(item.color) ||
                !Number.isInteger(item.fftSize) || item.fftSize < 2 || item.fftSize > 524288 ||
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
        this.forgetUndo();
        for (const item of parsed) {
            const base = item.name; let number = 2;
            while (this.items.some(existing => existing.name === item.name)) {
                item.name = base.slice(0, 72) + ' (' + number++ + ')';
            }
            if (this.items.some(existing => existing.color === item.color)) {
                item.color = this.colors.find(color => !this.items.some(existing => existing.color === color)) || item.color;
            }
            this.items.push(item);
        }
        this.onChange();
    }
}

class FFTReferencePanel {
    private disposed = false;
    constructor(private document: Document, private references: FFTReferences, private redraw: () => void,
                private recapture: (item: FFTReference) => void) {
        references.onChange = () => { this.render(); redraw(); };
        document.getElementById('save-references').addEventListener('click', () => {
            const url = URL.createObjectURL(new Blob([references.serialize()], {type:'application/json'}));
            const link = document.createElement('a'); link.href = url; link.download = 'koheron-' + references.board + '-references-' + new Date().toISOString().replace(/[:.]/g, '-') + '.json';
            link.click(); this.message('Download started · includes all ' + references.items.length + ' references.'); window.setTimeout(() => URL.revokeObjectURL(url), 1000);
        });
        document.getElementById('undo-reference').addEventListener('click', () => references.undo());
        const file = document.getElementById('reference-file') as HTMLInputElement;
        document.getElementById('load-references').addEventListener('click', () => file.click());
        file.addEventListener('change', async () => {
            const selected = file.files?.[0]; file.value = '';
            if (!selected) { return; }
            try {
                if (selected.size > 32 * 1024 * 1024) { throw new Error('Reference files must be smaller than 32 MB.'); }
                const text = await selected.text();
                if (this.disposed) { return; }
                const before = references.items.length;
                references.load(text);
                this.message('Added ' + (references.items.length - before) + ' references from ' + selected.name);
            } catch (error) { if (!this.disposed) { this.message(error instanceof Error ? error.message : 'Unable to load references.'); } }
        });
        this.render();
    }

    private message(text: string): void { this.document.getElementById('reference-message').textContent = text; }

    private render(): void {
        const d = this.document, list = d.getElementById('reference-list');
        list.replaceChildren(); this.message('');
        d.getElementById('reference-count').textContent = this.references.items.length + ' / ' + FFTReferences.limit;
        d.getElementById('reference-undo').hidden = !this.references.undoLabel;
        d.getElementById('reference-undo-label').textContent = this.references.undoLabel;
        (d.getElementById('save-references') as HTMLButtonElement).disabled = !this.references.items.length;
        for (const item of this.references.items) {
            const row = d.createElement('div'); row.className = 'reference-row';
            const toggle = d.createElement('input'); toggle.type = 'checkbox'; toggle.checked = item.visible;
            toggle.setAttribute('aria-label', 'Show ' + item.name); toggle.style.accentColor = item.color;
            toggle.addEventListener('change', () => { item.visible = toggle.checked; this.redraw(); });
            const name = d.createElement('input'); name.type = 'text'; name.value = item.name; name.maxLength = 80;
            name.setAttribute('aria-label', 'Reference name'); name.style.borderLeft = '3px solid ' + item.color;
            const rename = () => {
                item.name = name.value.trim() || item.name; name.value = item.name;
                toggle.setAttribute('aria-label', 'Show ' + item.name); remove.setAttribute('aria-label', 'Remove ' + item.name); replace.setAttribute('aria-label', 'Recapture ' + item.name); this.redraw();
            };
            name.addEventListener('change', rename);
            name.addEventListener('keydown', event => {
                if (event.key === 'Enter') { event.preventDefault(); rename(); name.blur(); }
                if (event.key === 'Escape') { event.preventDefault(); name.value = item.name; name.blur(); }
            });
            const actions = d.createElement('div'); actions.className = 'reference-row-actions';
            const replace = d.createElement('button'); replace.type = 'button'; replace.className = 'replace-reference';
            replace.textContent = 'Recapture'; replace.title = 'Replace this capture with the displayed live spectrum';
            replace.setAttribute('aria-label', 'Recapture ' + item.name);
            replace.addEventListener('click', () => this.recapture(item));
            const remove = d.createElement('button'); remove.type = 'button'; remove.textContent = '×'; remove.className = 'remove-reference'; remove.title = 'Remove reference';
            remove.setAttribute('aria-label', 'Remove ' + item.name);
            remove.addEventListener('click', () => this.references.remove(item));
            const metadata = d.createElement('small');
            const windows = ['Rectangular', 'Hann', 'Flat top', 'Blackman–Harris'];
            const channel = item.status.spectrum && item.status.channel >= 2 ? (item.status.channel === 2 ? 'ADC 0 − 1' : 'ADC 0 + 1') : 'ADC ' + item.status.channel;
            metadata.textContent = channel + ' · ' + windows[item.status.window_index] + ' · ' + item.status.fs / 1e6 + ' MS/s';
            metadata.title = metadata.textContent + ' · ' + item.fftSize + ' point FFT · ' + (item.status.clkIndex === '0' ? 'External' : item.status.clkIndex === 'fixed' ? 'Fixed' : 'Internal') + ' clock';
            const time = d.createElement('time'); time.dateTime = item.capturedAt;
            const captured = new Date(item.capturedAt), today = new Date();
            time.textContent = captured.toDateString() === today.toDateString()
                ? captured.toLocaleTimeString([], {hour:'2-digit', minute:'2-digit', second:'2-digit'})
                : captured.toLocaleDateString([], {year:'numeric', month:'short', day:'numeric'});
            time.title = 'Captured ' + new Date(item.capturedAt).toLocaleString();
            actions.append(replace, remove); row.append(toggle, name, time, metadata, actions); list.append(row);
        }
    }

    dispose(): void { this.disposed = true; this.references.onChange = () => {}; }
}
