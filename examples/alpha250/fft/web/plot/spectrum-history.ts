// History of received spectra, independent of browser paint cadence.
interface HistoryRow { bucket: number; psd: Float32Array; version: number; }
class SpectrumHistory {
    readonly interval = .05;
    // Keep the partially visible oldest row as the 30-second window scrolls.
    readonly maxRows = 601;
    readonly maxDensityFrames = 2048;
    rows: HistoryRow[] = [];
    average: Float32Array;
    maximum: Float32Array;
    density: Uint16Array;
    densityLow: Uint16Array;
    densityHigh: Uint16Array;
    densityFrames = 0;
    duration = 15;
    status: IFFTStatus;
    now = 0;
    epoch = 0;
    samples = 0;
    minPower = Infinity;
    maxPower = 0;
    private signature = '';
    private frames: {time: number; codes: Uint8Array; included: boolean}[] = [];

    get currentBucket(): number {
        const position = this.now / this.interval, nearest = Math.round(position);
        // Decimal 50 ms boundaries can divide just below their integer bucket.
        // Snap only floating-point roundoff, preserving actual fractional time.
        return Math.abs(position - nearest) <= 4 * Number.EPSILON * Math.max(1, Math.abs(position))
            ? nearest : Math.floor(position);
    }
    get bucketPhase(): number { return Math.max(0, Math.min(1, this.now / this.interval - this.currentBucket)); }

    reset(): void {
        this.rows = []; this.frames = [];
        this.average = this.maximum = this.density = this.densityLow = this.densityHigh = undefined;
        this.status = undefined; this.signature = '';
        this.densityFrames = this.samples = 0;
        this.minPower = Infinity; this.maxPower = 0;
        this.epoch++;
    }

    static code(power: number): number {
        if (!(power > 0) || !Number.isFinite(power)) { return 0; }
        return Math.max(1, Math.min(255, 1 + Math.round((10 * Math.log10(power / 1e-3) + 200) * 254 / 220)));
    }
    static power(code: number): number { return 1e-3 * Math.pow(10, (-200 + (code - 1) * 220 / 254) / 10); }

    setDuration(seconds: number): void {
        if (![5, 15, 30].includes(seconds) || seconds === this.duration) { return; }
        this.duration = seconds;
        if (!this.density) { return; }
        this.density.fill(0); this.densityLow.fill(256); this.densityHigh.fill(0); this.densityFrames = 0;
        for (const frame of this.frames) {
            frame.included = frame.time >= this.now - seconds;
            if (frame.included) { this.addCodes(frame.codes); }
        }
    }

    private addCodes(codes: Uint8Array): void {
        for (let i = 0; i < codes.length; i++) {
            const code = codes[i]; if (!code) { continue; }
            this.density[i * 256 + code]++;
            this.densityLow[i] = Math.min(this.densityLow[i], code);
            this.densityHigh[i] = Math.max(this.densityHigh[i], code);
        }
        this.densityFrames++;
    }
    private removeCodes(codes: Uint8Array): void {
        for (let i = 0; i < codes.length; i++) {
            const code = codes[i]; if (!code) { continue; }
            const base = i * 256; this.density[base + code]--;
            while (this.densityLow[i] <= this.densityHigh[i] && !this.density[base + this.densityLow[i]]) { this.densityLow[i]++; }
            while (this.densityHigh[i] >= this.densityLow[i] && !this.density[base + this.densityHigh[i]]) { this.densityHigh[i]--; }
        }
        this.densityFrames--;
    }

    add(psd: Float32Array, status: IFFTStatus, time: number): void {
        const signature = [psd.length, status.fs, status.channel, status.window_index, status.W1, status.W2, status.clkIndex].join('/');
        if (signature !== this.signature) {
            this.reset(); this.signature = signature;
            this.status = {...status, dds_freq: status.dds_freq.slice()};
            this.average = new Float32Array(psd.length).fill(NaN);
            this.maximum = new Float32Array(psd.length).fill(NaN);
            this.density = new Uint16Array(psd.length * 256);
            this.densityLow = new Uint16Array(psd.length).fill(256);
            this.densityHigh = new Uint16Array(psd.length);
        }
        // One-second exponential averaging of linear PSD, never of dB values.
        const weight = this.samples ? 1 - Math.exp(-Math.max(0, time - this.now)) : 1;
        this.now = time; this.samples++;
        const bucket = this.currentBucket;
        let row = this.rows[this.rows.length - 1];
        if (!row || row.bucket !== bucket) {
            row = {bucket, psd: new Float32Array(psd.length).fill(NaN), version: 0};
            this.rows.push(row);
        }
        const codes = new Uint8Array(psd.length);
        for (let i = 0; i < psd.length; i++) {
            const value = psd[i];
            if (!Number.isFinite(value) || value < 0) { continue; }
            this.average[i] = Number.isFinite(this.average[i]) ? this.average[i] + weight * (value - this.average[i]) : value;
            this.maximum[i] = Number.isFinite(this.maximum[i]) ? Math.max(this.maximum[i], value) : value;
            row.psd[i] = Number.isFinite(row.psd[i]) ? Math.max(row.psd[i], value) : value;
            if (value > 0) { this.minPower = Math.min(this.minPower, value); this.maxPower = Math.max(this.maxPower, value); }
            codes[i] = SpectrumHistory.code(value);
        }
        row.version++;
        this.frames.push({time, codes, included: true}); this.addCodes(codes);
        while (this.frames.length && (this.frames.length > this.maxDensityFrames || this.frames[0].time < time - 30)) {
            const expired = this.frames.shift(); if (expired.included) { this.removeCodes(expired.codes); }
        }
        for (const frame of this.frames) {
            if (frame.time >= time - this.duration) { break; }
            if (frame.included) { this.removeCodes(frame.codes); frame.included = false; }
        }
        while (this.rows.length && (this.rows.length > this.maxRows || this.rows[0].bucket < bucket - this.maxRows + 1)) { this.rows.shift(); }
    }
}
