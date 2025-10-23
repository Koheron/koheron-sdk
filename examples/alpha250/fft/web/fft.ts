// Interface for the FFT driver
// (c) Koheron

type TupleGetParameters = [number, number, number, number, number, number, number, number];
type TupleBoardParameters = [number, number, number, number];

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

class FFT {
    private driver: Driver;
    private id: number;
    private cmds: Commands;

    public fft_size: number;
    public status: IFFTStatus;

    constructor (private client: Client) {
        this.driver = this.client.getDriver('FFT');
        this.id = this.driver.id;
        this.cmds = this.driver.getCmds();
        //this.monitor(1000);

        this.status = <IFFTStatus>{};
        this.status.dds_freq = [];
    }

    init(cb: () => void): void {
        this.getFFTSize( async (size: number) => {
            this.fft_size = size;
            await this.getControlParameters();
            cb();
        });
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

    async getControlParameters(): Promise<IFFTStatus> {
        const [fdds0, fdds1, fs, channel, W1, W2, window_index, clkin] =
        await this.client.readTuple<TupleGetParameters>(
            Command(this.id, this.cmds['get_control_parameters']),
            'dddIddI'
        );

        let clkIndex: string = "0";

        if (clkin !== 0) {
            clkIndex = "2";
        }

        this.status = {dds_freq: [fdds0, fdds1], fs, channel, W1, W2, window_index, clkIndex};
        return this.status;
    }

    async getBoardParameters(): Promise<IBoardParameters> {
        const arr = await this.client.readFloat32Array(
            Command(this.id, this.cmds['get_board_parameters']));

        const supplyValues = arr.slice(0, 4);
        const adcValues    = arr.slice(4, 12);
        const dacValues    = arr.slice(12, 16);
        const temperatures = arr.slice(16, 19);

        return {supplyValues, adcValues, dacValues, temperatures};
    }
}
