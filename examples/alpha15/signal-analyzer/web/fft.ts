// Adapt the Alpha15 voltage spectra to the shared FFT workspace.
class FFT extends FFTDriver {
    public get waitingForSpectrum(): boolean {
        return this.controlsPending || this.delivered.some(sequence => sequence === 0);
    }
    // The three server snapshots have independent acquisition periods.
    public startPSDStream = undefined;
    private decimator: Decimator;
    private ranges: Ltc2387;
    private clock: ClockGenerator;
    private snapshots: (Float32Array | Float64Array)[] = [];
    private fetchedAt = [-Infinity, -Infinity, -Infinity];
    private revision = 0;
    private controlsPending = false;
    private refreshOnly = false;
    private controlRequest = 0;
    private generations: number[];
    private sequences = [0, 0, 0];
    private delivered = [0, 0, 0];
    private segments: {first: number; last: number; fs: number; size: number}[];

    constructor(client: Client) {
        super(client);
        this.decimator = new Decimator(client);
        this.ranges = new Ltc2387(client);
        this.clock = new ClockGenerator(client);
    }

    async init(): Promise<void> {
        await this.decimator.init();
        await super.init();
        const low = this.decimator.status;
        const configs = [
            {fs: low.fs_lf, size: low.n_pts, bins: low.n_pts / 2 + 1, first: 0, trim: 550},
            {fs: low.fs, size: low.n_pts, bins: low.n_pts / 2 + 1, first: 220, trim: 600},
            {fs: this.status.fs, size: this.fft_size, bins: this.fft_size / 2, first: 110, trim: 0}
        ];
        let previous = -1;
        this.segments = configs.map(config => {
            const first = Math.max(config.first, Math.floor(previous * config.size / config.fs) + 1);
            const last = config.bins - 1 - config.trim;
            previous = last * config.fs / config.size;
            return {first, last, fs: config.fs, size: config.size};
        });
        await this.getControlParameters();
    }

    async read_psd(): Promise<Float32Array> {
        const frame = await this.readSpectrum();
        return frame ? frame.psd : new Float32Array(this.status.spectrum.frequencies.length).fill(NaN);
    }

    async readSpectrum(): Promise<SpectrumFrame | undefined> {
        if (this.controlsPending) { return undefined; }
        const revision = this.revision, status = this.status;
        const now = performance.now();
        const providers = [() => this.decimator.readSnapshot(0), () => this.decimator.readSnapshot(1),
            () => this.client.readTupleWithFloat32Vector(Command(this.id, this.cmds['get_spectrum_snapshot']), 'IQ', 12)];
        const snapshots = await Promise.all(this.segments.map(async (segment, band) => {
            const interval = band === 2 ? 0 : 1000 * segment.size / segment.fs;
            if (now - this.fetchedAt[band] >= interval) {
                const {metadata, values} = await providers[band]();
                if (revision !== this.revision) { return undefined; }
                const [generation, sequence] = metadata;
                if (generation > this.generations[band]) {
                    // Another client restarted acquisition. Refresh controls
                    // before accepting its frames, without restarting it again.
                    this.generations[band] = generation;
                    this.settingsChanged(true);
                    this.sequences = [0, 0, 0];
                    this.delivered = [0, 0, 0];
                    return undefined;
                }
                if (generation !== this.generations[band] || !sequence) {
                    this.fetchedAt[band] = performance.now();
                    this.snapshots[band] = undefined;
                    return undefined;
                }
                if (values.length <= segment.last) { throw new Error('Incomplete spectrum band'); }
                this.snapshots[band] = values.slice();
                this.sequences[band] = sequence;
                this.fetchedAt[band] = performance.now();
            }
            return this.snapshots[band];
        }));
        // Never repopulate the cache or label an older request with newer settings.
        if (revision !== this.revision) { return undefined; }
        // A stitched frame is delivered once, after every band has advanced.
        // Faster bands can update the cache while the slowest band is pending.
        if (snapshots.some(values => !values) || this.sequences.some((sequence, band) => sequence === this.delivered[band])) {
            return undefined;
        }
        const psd = new Float32Array(status.spectrum.frequencies.length);
        let offset = 0;
        this.segments.forEach((segment, band) => {
            if (snapshots[band].length <= segment.last) { throw new Error('Incomplete spectrum band'); }
            for (let i = segment.first; i <= segment.last; i++) { psd[offset++] = snapshots[band][i]; }
        });
        this.delivered = this.sequences.slice();
        return {psd, status};
    }

    settingsChanged(refreshOnly = false): void {
        this.revision++;
        this.controlsPending = true;
        this.refreshOnly = refreshOnly;
        this.snapshots = [];
        this.fetchedAt = [-Infinity, -Infinity, -Infinity];
    }

    setInputChannel(channel: number): void {
        this.settingsChanged();
        const value = Number(channel);
        if (value >= 2) { this.client.send(Command(this.id, this.cmds['set_operation'], value - 2)); }
        this.client.send(Command(this.id, this.cmds['select_adc_channel'], Math.min(value, 2)));
    }

    setFFTWindow(windowIndex: number): void {
        this.settingsChanged();
        super.setFFTWindow(Number(windowIndex));
        this.decimator.setFFTWindow(Number(windowIndex));
    }

    async getControlParameters(): Promise<IFFTStatus> {
        const revision = this.revision, request = ++this.controlRequest;
        const [tuple, windowIndex, range0, range1, reference] = await Promise.all([
            this.client.readTuple(Command(this.id, this.cmds['get_control_parameters']), 'dIIddd'),
            this.client.readUint32(Command(this.id, this.cmds['get_window_index'])),
            this.ranges.inputRange(0), this.ranges.inputRange(1), this.clock.getReferenceClock()
        ]);
        if (revision !== this.revision || request !== this.controlRequest) { return this.status; }
        const status: IFFTStatus = {
            fs: tuple[0], channel: tuple[1] === 2 ? 2 + tuple[2] : tuple[1],
            W1: tuple[3] / (this.fft_size * this.fft_size), W2: tuple[4] / this.fft_size,
            window_index: windowIndex, clkIndex: reference === 0 ? '0' : '2', dds_freq: [],
            inputRanges: [range0 ? 8.192 : 2.048, range1 ? 8.192 : 2.048],
            acquisitionKey: [range0, range1, tuple[2]].join('/')
        };
        if (this.segments) {
            // The server reports S2/S1; multiply by each band sampling rate for Hz.
            if (this.status.spectrum && this.status.window_index === windowIndex && this.status.W1 === status.W1 && this.status.W2 === status.W2) {
                status.spectrum = this.status.spectrum;
            } else {
                const frequencies: number[] = [], bandwidths: number[] = [];
                this.segments.forEach(segment => {
                    for (let i = segment.first; i <= segment.last; i++) {
                        frequencies.push(i * segment.fs / segment.size);
                        bandwidths.push(tuple[5] * segment.fs);
                    }
                });
                status.spectrum = {frequencies: Object.freeze(frequencies), bandwidths: Object.freeze(bandwidths),
                    binSpacings: Object.freeze(this.segments.map(segment => segment.fs / segment.size)),
                    unit: 'Hz', logarithmic: true};
            }
        }
        const settings = ['acquisitionKey', 'channel', 'window_index', 'clkIndex', 'fs', 'W1', 'W2'] as (keyof IFFTStatus)[];
        if ((this.controlsPending && !this.refreshOnly) || !this.generations || settings.some(key => !Object.is(status[key], this.status[key]))) {
            this.revision++;
            this.snapshots = [];
            this.fetchedAt = [-Infinity, -Infinity, -Infinity];
            this.controlsPending = true;
            const restartRevision = this.revision;
            const [decimator, rf] = await Promise.all([
                this.decimator.restartAcquisition(),
                this.client.readUint32(Command(this.id, this.cmds['restart_acquisition']))
            ]);
            if (restartRevision !== this.revision || request !== this.controlRequest) { return this.status; }
            this.generations = [decimator, decimator, rf];
            this.sequences = [0, 0, 0];
            this.delivered = [0, 0, 0];
        }
        this.status = status;
        this.controlsPending = false;
        this.refreshOnly = false;
        return status;
    }
}
