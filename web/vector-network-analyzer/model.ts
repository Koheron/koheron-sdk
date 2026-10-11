interface VNASettings { start: number; stop: number; points: number; duration: number; level: number; record?: 'auto' | 'full'; }
interface VNAFrame {
    source: 'hardware' | 'demo';
    settings: VNASettings; timestamp: string; frequency: number[];
    real: (number | null)[]; imag: (number | null)[];
    recordSamples?: number;
}
interface VNATrace extends PlotReference { frame: VNAFrame; calibrated: boolean; }
interface VNADisplay { frame: VNAFrame; magnitude: number[]; phase: number[]; delay: number[]; calibrated: boolean; }

// The shared collection owns presentation, limits, undo and file append. This
// adapter freezes the complex response before display-only port extension.
class VNAReferences extends PlotReferences<VNATrace> {
    constructor(private source?: VNAFrame['source']) { super(VNAProfile.board); }
    capture(frame: VNAFrame, calibrated: boolean, name = ''): void {
        this.add({frame: VNAModel.validateFrame(frame), calibrated});
        if (name.trim()) { this.items[this.items.length - 1].name = name.trim().slice(0, 80); this.onChange(); }
    }
    replace(item: VNATrace, frame: VNAFrame, calibrated: boolean): void {
        this.replaceCapture(item, {frame: VNAModel.validateFrame(frame), calibrated});
    }
    // Session restore replaces the collection; file append alone resolves name
    // and color collisions. An empty append resets the shared undo history.
    restore(items: VNATrace[]): void { this.items = items; this.append([]); }
    serialize(): string {
        return JSON.stringify({format: 'koheron-vna-references', version: 1, board: this.board, references: this.items});
    }
    load(text: string): void {
        const parsed = this.parseFile(text, 'koheron-vna-references').map(item => {
            if (typeof item.calibrated !== 'boolean') throw new Error('Invalid reference correction state.');
            const frame = VNAModel.validateFrame(item.frame);
            if (this.source && frame.source !== this.source)
                throw new Error('Hardware and synthetic demo references are kept separate.');
            return {name: item.name.trim(), capturedAt: item.capturedAt, visible: item.visible,
                color: item.color, calibrated: item.calibrated, frame};
        });
        this.append(parsed);
    }
}

class VNAModel {
    static get FS(): number { return VNAProfile.sampleRate; }
    static get SOURCE_SHIFT(): number { return VNAProfile.sourceShift; }
    static readonly SAMPLES = VNAProtocol.captureSamples;
    static readonly MAX_POINTS = 2001;
    settings: VNASettings = {start: 1000, stop: VNAProfile.maxFrequency, points: 501, duration: .250, level: -6, record: 'auto'};
    latest: VNAFrame | null = null;
    calibration: VNAFrame | null = null;
    readonly referenceCollection: VNAReferences;
    get references(): VNATrace[] { return this.referenceCollection.items; }
    applyCalibration = true;
    unwrap = false;
    delayNs = 0;
    constructor(private source?: VNAFrame['source']) { this.referenceCollection = new VNAReferences(source); }

    static validateSettings(s: VNASettings): VNASettings {
        if (!s || ![s.start, s.stop, s.points, s.duration, s.level].every(Number.isFinite) ||
            s.start < 1000 || s.stop > VNAProfile.maxFrequency || s.stop <= s.start ||
            !Number.isInteger(s.points) || s.points < 51 || s.points > this.MAX_POINTS ||
            s.duration < .020 || s.duration > .250 || s.level < -40 || s.level > 0 ||
            (s.record !== undefined && !['auto', 'full'].includes(s.record)))
            throw new Error(`Use 1 kHz–${VNAProfile.maxFrequency / 1e6} MHz, 51–2001 points, 20–250 ms and −40–0 dBFS. Start must be below stop.`);
        if (Math.round(s.stop * VNAModel.SAMPLES / VNAModel.FS) <= Math.round(s.start * VNAModel.SAMPLES / VNAModel.FS))
            throw new Error(`The span is smaller than the ${(VNAModel.FS / VNAModel.SAMPLES).toFixed(2)} Hz FFT-bin spacing. Increase the span.`);
        if (s.duration < VNAModel.minimumDuration(s.start, s.stop))
            throw new Error(`This span needs a chirp longer than ${(VNAModel.minimumDuration(s.start, s.stop) * 1000).toFixed(1)} ms. Choose a longer duration or narrower span.`);
        return {...s};
    }
    // Older sessions used the full record. Preserve their frequency grid and
    // calibration until the user explicitly selects automatic record length.
    static recordSamples(settings: VNASettings): number {
        if (settings.record !== 'auto') return this.SAMPLES;
        let n = Math.max(4 * 1024 * 1024, 2 ** Math.ceil(Math.log2(this.FS * (settings.duration + .010))));
        n = Math.min(n, this.SAMPLES);
        while (n < this.SAMPLES && Math.round(settings.stop * n / this.FS) <= Math.round(settings.start * n / this.FS)) n *= 2;
        return n;
    }
    static frequencyBins(settings: VNASettings, n = this.recordSamples(settings)): number[] {
        return Array.from(new Set(Array.from({length: settings.points}, (_, i) =>
            Math.round(settings.start * (settings.stop / settings.start) ** (i / (settings.points - 1)) * n / this.FS))));
    }
    static minimumDuration(start: number, stop: number): number {
        return (16 * Math.log(2.2 * stop / start) / Math.log1p(2 ** -16) + 2) / VNAModel.FS;
    }
    static sameSettings(a: VNASettings, b: VNASettings): boolean {
        return ['start', 'stop', 'points', 'duration', 'level'].every(k => a[k] === b[k]) && this.recordSamples(a) === this.recordSamples(b);
    }
    static validateFrame(input: any): VNAFrame {
        const settings = VNAModel.validateSettings(input?.settings);
        const n = input.recordSamples ?? VNAModel.SAMPLES;
        if (!Number.isInteger(n) || n < 4 * 1024 * 1024 || n > VNAModel.SAMPLES ||
            (n & (n - 1)) !== 0 || Math.round(settings.duration * VNAModel.FS) > n - 1024 ||
            n !== VNAModel.recordSamples(settings)) throw new Error('Invalid measurement record length.');
        const f = input.frequency, re = input.real, im = input.imag;
        if (!Array.isArray(f) || !Array.isArray(re) || !Array.isArray(im) ||
            f.length < 2 || f.length > settings.points || re.length !== f.length || im.length !== f.length ||
            !['hardware', 'demo'].includes(input.source) ||
            typeof input.timestamp !== 'string' || !Number.isFinite(Date.parse(input.timestamp)))
            throw new Error('Invalid measurement file.');
        for (let i = 0; i < f.length; ++i) {
            if (!Number.isFinite(f[i]) || f[i] <= 0 || f[i] >= VNAModel.FS / 2 ||
                (i > 0 && f[i] <= f[i-1]) ||
                !((re[i] === null && im[i] === null) ||
                  (Number.isFinite(re[i]) && Number.isFinite(im[i]))))
                throw new Error('Measurement contains invalid frequencies or complex values.');
        }
        return {source: input.source, settings, timestamp: input.timestamp, frequency: f.slice(), real: re.slice(), imag: im.slice(),
            ...(input.recordSamples !== undefined ? {recordSamples: n} : {})};
    }
    calibrationMatches(frame: VNAFrame | null = null): boolean {
        const c = this.calibration;
        if (!c || !VNAModel.sameSettings(c.settings, frame?.settings || this.settings)) return false;
        if ((c.recordSamples ?? VNAModel.SAMPLES) !== (frame ? frame.recordSamples ?? VNAModel.SAMPLES : VNAModel.recordSamples(this.settings))) return false;
        return !frame || (c.source === frame.source && c.frequency.length === frame.frequency.length &&
            c.frequency.every((f, i) => f === frame.frequency[i]));
    }
    display(frame: VNAFrame, allowCalibration = true, applyExtension = true): VNADisplay {
        const calibrated = allowCalibration && this.applyCalibration && this.calibrationMatches(frame);
        const re: (number | null)[] = [], im: (number | null)[] = [];
        for (let i = 0; i < frame.frequency.length; ++i) {
            let a = frame.real[i], b = frame.imag[i];
            if (calibrated && a !== null && b !== null) {
                const c = this.calibration.real[i], d = this.calibration.imag[i];
                const power = c === null || d === null ? 0 : c*c + d*d;
                if (power < 1e-12) { a = b = null; }
                else { const r = (a*c + b*d) / power; b = (b*c - a*d) / power; a = r; }
            }
            if (a !== null && b !== null && this.delayNs && applyExtension) {
                const angle = 2 * Math.PI * frame.frequency[i] * this.delayNs * 1e-9;
                const r = a * Math.cos(angle) - b * Math.sin(angle);
                b = a * Math.sin(angle) + b * Math.cos(angle); a = r;
            }
            re.push(a); im.push(b);
        }
        const magnitude = re.map((a, i) => a === null ? NaN : 20 * Math.log10(Math.max(1e-15, Math.hypot(a, im[i]))));
        const wrapped = re.map((a, i) => a === null ? NaN : Math.atan2(im[i], a));
        const unwrapped = wrapped.slice();
        let previous = NaN, offset = 0;
        for (let i = 0; i < unwrapped.length; ++i) {
            if (!Number.isFinite(wrapped[i])) { previous = NaN; offset = 0; continue; }
            if (Number.isFinite(previous)) {
                const delta = wrapped[i] - previous;
                if (delta > Math.PI) offset -= 2 * Math.PI;
                if (delta < -Math.PI) offset += 2 * Math.PI;
            }
            unwrapped[i] += offset; previous = wrapped[i];
        }
        const delay = unwrapped.map((_, i) => {
            const low = Math.max(0, i-1), high = Math.min(unwrapped.length-1, i+1);
            return Number.isFinite(unwrapped[low]) && Number.isFinite(unwrapped[high]) && Number.isFinite(unwrapped[i])
                ? -(unwrapped[high] - unwrapped[low]) / (2 * Math.PI * (frame.frequency[high] - frame.frequency[low])) * 1e9 : NaN;
        });
        return {frame: {...frame, real: re, imag: im}, magnitude,
            phase: (this.unwrap ? unwrapped : wrapped).map(v => v * 180 / Math.PI), delay, calibrated};
    }
    captureReference(name = ''): void {
        if (!this.latest) throw new Error('Measure a device before saving a trace.');
        const current = this.display(this.latest, true, false);
        this.referenceCollection.capture(current.frame, current.calibrated, name);
    }
    replaceReference(item: VNATrace): void {
        if (!this.latest) throw new Error('Measure a device before recapturing a reference.');
        const current = this.display(this.latest, true, false);
        this.referenceCollection.replace(item, current.frame, current.calibrated);
    }
    serialize(): string {
        return JSON.stringify({format: `koheron-${VNAProfile.board}-vna`, version: 1, settings: this.settings,
            latest: this.latest, calibration: this.calibration, references: this.references,
            display: {applyCalibration: this.applyCalibration, unwrap: this.unwrap, delayNs: this.delayNs}});
    }
    load(text: string): void {
        const data = JSON.parse(text);
        if (data?.format !== `koheron-${VNAProfile.board}-vna` || data.version !== 1 ||
            !Array.isArray(data.references) || data.references.length > PlotReferences.limit) throw new Error('Unsupported analyzer session.');
        const settings = VNAModel.validateSettings(data.settings);
        const latest = data.latest === null ? null : VNAModel.validateFrame(data.latest);
        const calibration = data.calibration === null ? null : VNAModel.validateFrame(data.calibration);
        const references = data.references.map(t => {
            if (typeof t.name !== 'string' || !t.name.trim() || t.name.length > 80 ||
                typeof t.visible !== 'boolean' || typeof t.calibrated !== 'boolean')
                throw new Error('Invalid saved trace.');
            const frame = VNAModel.validateFrame(t.frame);
            const capturedAt = t.capturedAt || frame.timestamp;
            if (typeof capturedAt !== 'string' || !Number.isFinite(Date.parse(capturedAt))) throw new Error('Invalid reference timestamp.');
            return {name: t.name.trim(), capturedAt, color: /^#[0-9a-f]{6}$/i.test(t.color) ? t.color : '#a178b5',
                visible: t.visible, calibrated: t.calibrated, frame};
        });
        const display = data.display || {applyCalibration: true, unwrap: false, delayNs: 0};
        if (typeof display.applyCalibration !== 'boolean' || typeof display.unwrap !== 'boolean' ||
            !Number.isFinite(display.delayNs) || Math.abs(display.delayNs) > 1e6) throw new Error('Invalid display settings.');
        if (this.source && [latest, calibration, ...references.map(t => t.frame)].filter(Boolean).some(f => f.source !== this.source))
            throw new Error('Hardware and synthetic demo sessions are kept separate.');
        this.settings = settings; this.latest = latest; this.calibration = calibration;
        this.applyCalibration = display.applyCalibration; this.unwrap = display.unwrap; this.delayNs = display.delayNs;
        this.referenceCollection.restore(references);
    }
    csv(): string {
        if (!this.latest) throw new Error('No measurement to export.');
        const d = this.display(this.latest), f = d.frame;
        const rows = [`# ${VNAProfile.label} forward transmission; source: ${f.source}; complex thru correction: ${d.calibrated}`,
            `# UTC: ${f.timestamp}; level_dBFS: ${f.settings.level}; sweep_s: ${f.settings.duration}; record_samples: ${f.recordSamples ?? VNAModel.SAMPLES}; port_extension_ns: ${this.delayNs}; phase: ${this.unwrap ? 'unwrapped' : 'wrapped'}`,
            'frequency_Hz,real,imaginary,magnitude_dB,phase_deg,group_delay_ns'];
        f.frequency.forEach((v, i) => rows.push([v, f.real[i], f.imag[i], d.magnitude[i], d.phase[i], d.delay[i]]
            .map(n => n !== null && Number.isFinite(n) ? n : '').join(',')));
        return rows.join('\r\n') + '\r\n';
    }
}

function vnaFrequency(hz: number): string {
    return hz >= 1e6 ? `${(hz / 1e6).toLocaleString('en', {maximumFractionDigits: 3})} MHz`
        : `${(hz / 1e3).toLocaleString('en', {maximumFractionDigits: 3})} kHz`;
}
