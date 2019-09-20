// Interface for the FFT driver
// (c) Koheron

interface IFFTStatus {
    fs: number[]; // Sampling frequencies (Hz)
    channel: number; // Input channel
    W1: number; // FFT window correction (sum w)^2
    W2: number; // FFT window correction (sum w^2)
}

class FFT {
    private driver: Driver;
    private id: number;
    private cmds: Commands;

    public fft_size: number;
    public status: IFFTStatus;
    public adc_input: number;

    constructor (private client: Client) {
        this.driver = this.client.getDriver('FFT');
        this.id = this.driver.id;
        this.cmds = this.driver.getCmds();
        //this.monitor(1000);

        this.status = <IFFTStatus>{};
        this.status.fs = [];
        this.adc_input = 0;
    }

    init(cb: () => void): void {
        this.getFFTSize( (size: number) => {
            this.fft_size = size;
            this.getControlParameters( () => {
                cb();
            });
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

    read_psd_raw(adc: number, cb: (psd: Float32Array) => void): void {
        this.client.readFloat32Array(Command(this.id, this.cmds['read_psd_raw'], adc), (psd: Float32Array) => {
            cb(psd);
        });
    }

    read_psd(adc: number, cb: (psd: Float32Array) => void): void {
        this.client.readFloat32Array(Command(this.id, this.cmds['read_psd'], adc), (psd: Float32Array) => {
            cb(psd);
        });
    }

    setInputChannel(channel: number): void {
        this.client.send(Command(this.id, this.cmds['set_input_channel'], channel));
    }

    setFFTWindow(windowIndex: number): void {
        this.client.send(Command(this.id, this.cmds['set_fft_window'], windowIndex));
    }

    getControlParameters(cb: (status: IFFTStatus) => void): void {
        this.client.readTuple(Command(this.id, this.cmds['get_control_parameters']), 'ddIdd',
                               (tup: [number, number, number, number, number, number]) => {
            this.status.fs[0] = tup[0];
            this.status.fs[1] = tup[1];
            this.status.channel = tup[3];
            this.status.W1 = tup[4];
            this.status.W2 = tup[5];
            cb(this.status);
        });
    }

    getFFTWindowIndex(cb: (windowIndex: number) => void): void {
        this.client.readUint32(Command(this.id, this.cmds['get_window_index']),
                               (windowIndex: number) => {
            cb(windowIndex);
        });
    }

    setInputAdc(adc: number) {
        this.adc_input = adc;
    }
}
