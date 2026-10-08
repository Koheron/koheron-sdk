// Four physical inputs share two FPGA FFT engines and one channel selector.
class FFT extends FFTDriver {
    public startPSDStream = undefined;
    public get waitingForSpectrum(): boolean { return this.controlsPending; }
    private adc = 0;
    private revision = 0;
    private controlsPending = false;
    private controlRequest = 0;
    private clock: ClockGenerator;
    private precisionAdc: PrecisionAdc;
    private precisionDac: PrecisionDac;
    private temperature: TemperatureSensor;
    private power: PowerMonitor;

    constructor(client: Client) {
        super(client);
        this.clock = new ClockGenerator(client);
        this.precisionAdc = new PrecisionAdc(client);
        this.precisionDac = new PrecisionDac(client);
        this.temperature = new TemperatureSensor(client);
        this.power = new PowerMonitor(client);
    }

    settingsChanged(): void {
        this.revision++;
        this.controlsPending = true;
    }

    setInputChannel(channel: number): void {
        channel = Number(channel);
        if (!Number.isInteger(channel) || channel < 0 || channel > 3) { return; }
        this.settingsChanged();
        this.adc = channel >> 1;
        super.setInputChannel(channel & 1);
    }

    setFFTWindow(windowIndex: number): void {
        this.settingsChanged();
        super.setFFTWindow(Number(windowIndex));
    }

    async getControlParameters(): Promise<IFFTStatus> {
        const revision = this.revision, request = ++this.controlRequest, adc = this.adc;
        const [tuple, window_index, reference] = await Promise.all([
            this.client.readTuple<[number, number, number, number, number]>(
                Command(this.id, this.cmds['get_control_parameters']), 'ddIdd'),
            this.client.readUint32(Command(this.id, this.cmds['get_window_index'])),
            this.clock.getReferenceClock()
        ]);
        if (revision !== this.revision || request !== this.controlRequest) { return this.status; }
        const [fs0, fs1, channel, W1, W2] = tuple;
        this.status = {dds_freq: [], fs: adc === 0 ? fs0 : fs1, channel: adc * 2 + channel,
            W1, W2, window_index, clkIndex: reference === 0 ? '0' : '2'};
        this.controlsPending = false;
        return this.status;
    }

    async read_psd(): Promise<Float32Array> {
        return this.client.readFloat32Array(Command(this.id, this.cmds['read_psd'], this.adc));
    }

    async readSpectrum(): Promise<SpectrumFrame | undefined> {
        if (this.controlsPending) { return undefined; }
        const revision = this.revision, status = this.status;
        const psd = await this.read_psd();
        if (revision !== this.revision) { return undefined; }
        if (psd.length !== this.fft_size / 2) { throw new Error('Incomplete ALPHA250-4 spectrum'); }
        return {psd, status};
    }

    async getBoardParameters(): Promise<IBoardParameters> {
        const [supplyValues, adcValues, dacValues, temperatures] = await Promise.all([
            this.power.getSuppliesUI(), this.precisionAdc.getAdcValues(),
            this.precisionDac.getDacValues(), this.temperature.getTemperatures()
        ]);
        return {supplyValues, adcValues, dacValues, temperatures};
    }
}
