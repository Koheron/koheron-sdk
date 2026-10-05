// Interface for the FFT driver
// (c) Koheron

interface IFFTStatus {
    dds_freq: number[];
    fs: number; // Sampling frequency (Hz)
    channel: number; // Input channel
    W1: number; // FFT window correction (sum w)^2
    W2: number; // FFT window correction (sum w^2)
    window_index: number;
    clkIndex: string;
}

interface IBoardParameters {
    supplyValues: Float32Array;
    adcValues: Float32Array;
    dacValues: Float32Array;
    temperatures: Float32Array;
}

abstract class FFTDriver {
    protected driver: Driver;
    protected id: number;
    protected cmds: Commands;

    public fft_size: number;
    public status: IFFTStatus;

    constructor (protected client: Client) {
        this.driver = this.client.getDriver('FFT');
        this.id = this.driver.id;
        this.cmds = this.driver.getCmds();
        this.status = <IFFTStatus>{};
        this.status.dds_freq = [];
    }

    async init(): Promise<void> {
        this.fft_size = await this.client.readUint32(Command(this.id, this.cmds['get_fft_size']));
        await this.getControlParameters();
    }

    monitor(timeout: number): void {
        this.getCycleIndex( (i) => {
            setTimeout( () => {
                this.monitor(timeout);
            }, timeout);
        });
    }

    getCycleIndex(cb: (i: number) => void): void {
        this.client.readUint32(Command(this.id, this.cmds['get_cycle_index']),
                                 (i) => {cb(i)});
    }

    getFFTSize(cb: (size: number) => void): void {
        this.client.readUint32(Command(this.id, this.cmds['get_fft_size']),
                                 (size) => {cb(size)});
    }

    read_psd_raw(cb: (psd: Float32Array) => void): void {
        this.client.readFloat32Array(Command(this.id, this.cmds['read_psd_raw']), (psd: Float32Array) => {
            cb(psd);
        });
    }

    startPSDStream(frame: (psd: Float32Array, time: number) => void,
                   error: (message: string) => void): PSDStream {
        return new PSDStream(location.hostname, Command(this.id, this.cmds['read_psd']).data,
                             this.fft_size / 2, frame, error);
    }

    async read_psd(): Promise<Float32Array> {
        return await this.client.readFloat32Array(Command(this.id, this.cmds['read_psd']));
    }

    setDDSFreq(channel: number, freq_hz: number): void {
        this.client.send(Command(this.id, this.cmds['set_dds_freq'], channel, freq_hz));
    }

    setInputChannel(channel: number): void {
        this.client.send(Command(this.id, this.cmds['set_input_channel'], channel));
    }

    setFFTWindow(windowIndex: number): void {
        this.client.send(Command(this.id, this.cmds['set_fft_window'], windowIndex));
    }

    abstract getControlParameters(): Promise<IFFTStatus>;
}
