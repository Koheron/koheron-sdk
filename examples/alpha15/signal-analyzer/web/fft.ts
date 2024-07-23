// Interface for the FFT driver
// (c) Koheron

interface IFFTStatus {
    fs: number;      // Sampling frequency (Hz)
    channel: number; // Input channel
    S1: number;      // FFT window correction (sum w)^2
    S2: number;      // FFT window correction (sum w^2)
    ENBW: number;    // FFT window effective noise bandwidth
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

    read_psd_raw(cb: (psd: Float32Array) => void): void {
        this.client.readFloat32Array(Command(this.id, this.cmds['read_psd_raw']), (psd: Float32Array) => {
            cb(psd);
        });
    }

    read_psd(cb: (psd: Float32Array) => void): void {
        this.client.readFloat32Array(Command(this.id, this.cmds['read_psd']), (psd: Float32Array) => {
            cb(psd);
        });
    }

    setInputChannel(channel: number): void {
        if (channel == 2) { // 0 - 1
            this.client.send(Command(this.id, this.cmds['set_operation'], 0));
            this.client.send(Command(this.id, this.cmds['select_adc_channel'], 2));
        } else if (channel == 3) { // 0 + 1
            this.client.send(Command(this.id, this.cmds['set_operation'], 1));
            this.client.send(Command(this.id, this.cmds['select_adc_channel'], 2));
        } else { // Channel 0 or 1
            this.client.send(Command(this.id, this.cmds['select_adc_channel'], channel));
        }
    }

    setFFTWindow(windowIndex: number): void {
        this.client.send(Command(this.id, this.cmds['set_fft_window'], windowIndex));
    }

    getControlParameters(cb: (status: IFFTStatus) => void): void {
        this.client.readTuple(Command(this.id, this.cmds['get_control_parameters']), 'dIIddd',
                               (tup: [number, number, number, number, number, number]) => {
            this.status.fs = tup[0];
            let input_channel = tup[1];
            let input_operation = tup[2];
            this.status.S1 = tup[3];
            this.status.S2 = tup[4];
            this.status.ENBW = tup[5];

            if (input_channel == 2) {
                if (input_operation == 0) {
                    this.status.channel = 2;
                } else {
                    this.status.channel = 3;
                }
            } else {
                this.status.channel = input_channel;
            }

            cb(this.status);
        });
    }

    getFFTWindowIndex(cb: (windowIndex: number) => void): void {
        this.client.readUint32(Command(this.id, this.cmds['get_window_index']),
                               (windowIndex: number) => {
            cb(windowIndex);
        });
    }
}
