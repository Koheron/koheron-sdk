// Adapt the Alpha15 voltage spectra to the shared FFT workspace.
class FFT extends FFTDriver {
    // The three server snapshots have independent acquisition periods.
    public startPSDStream = undefined;
    private decimator: Decimator;
    private ranges: Ltc2387;
    private clock: ClockGenerator;
    private snapshots: (Float32Array | Float64Array)[] = [];
    private fetchedAt = [-Infinity, -Infinity, -Infinity];
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
        const now = performance.now();
        const providers = [() => this.decimator.spectralDensityLf(), () => this.decimator.spectralDensity(),
            () => this.client.readFloat32Array(Command(this.id, this.cmds['read_psd']))];
        const snapshots = await Promise.all(this.segments.map(async (segment, band) => {
            const interval = band === 2 ? 0 : 1000 * segment.size / segment.fs;
            if (!this.snapshots[band] || now - this.fetchedAt[band] >= interval) {
                this.snapshots[band] = await providers[band]();
                this.fetchedAt[band] = now;
            }
            return this.snapshots[band];
        }));
        const psd = new Float32Array(this.status.spectrum.frequencies.length);
        let offset = 0;
        this.segments.forEach((segment, band) => {
            if (snapshots[band].length <= segment.last) { throw new Error('Incomplete spectrum band'); }
            for (let i = segment.first; i <= segment.last; i++) { psd[offset++] = snapshots[band][i]; }
        });
        return psd;
    }

    setInputChannel(channel: number): void {
        this.snapshots = [];
        const value = Number(channel);
        if (value >= 2) { this.client.send(Command(this.id, this.cmds['set_operation'], value - 2)); }
        this.client.send(Command(this.id, this.cmds['select_adc_channel'], Math.min(value, 2)));
    }

    setFFTWindow(windowIndex: number): void {
        this.snapshots = [];
        super.setFFTWindow(Number(windowIndex));
        this.decimator.setFFTWindow(Number(windowIndex));
    }

    async getControlParameters(): Promise<IFFTStatus> {
        const [tuple, windowIndex, range0, range1, reference] = await Promise.all([
            this.client.readTuple(Command(this.id, this.cmds['get_control_parameters']), 'dIIddd'),
            this.client.readUint32(Command(this.id, this.cmds['get_window_index'])),
            this.ranges.inputRange(0), this.ranges.inputRange(1), this.clock.getReferenceClock()
        ]);
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
        if (status.acquisitionKey !== this.status.acquisitionKey || status.channel !== this.status.channel || status.window_index !== this.status.window_index || status.clkIndex !== this.status.clkIndex) {
            this.snapshots = [];
        }
        this.status = status;
        return status;
    }
}
