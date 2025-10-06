// Interface for the Decimator driver
// (c) Koheron

interface IDecimatorStatus {
    fs: number; // Sampling frequency (Hz)
    tx_duration: number; // FIFO transfer duration (s)
    cic_rate: number;
    n_pts: number;
}

class Decimator {
    private driver: Driver;
    private id: number;
    private cmds: Commands;

    public status: IDecimatorStatus;

    constructor (private client: Client) {
        this.driver = this.client.getDriver('Decimator');
        this.id = this.driver.id;
        this.cmds = this.driver.getCmds();

        this.status = <IDecimatorStatus>{};
    }

    init(cb: () => void): void {
        this.getControlParameters( () => {
            cb();
        });
    }

    setFFTWindow(windowIndex: number): void {
        this.client.send(Command(this.id, this.cmds['set_fft_window'], windowIndex));
    }

    read_adc(cb: (data: Float64Array) => void): void {
        this.client.readFloat64Array(Command(this.id, this.cmds['read_adc']),
            (data: Float64Array) => {
                cb(data);
            });
    }

    async spectralDensity(): Promise<Float64Array> {
        return await this.client.readFloat64Vector(Command(this.id, this.cmds['spectral_density']));
    }

    getControlParameters(cb: (status: IDecimatorStatus) => void): void {
        this.client.readTuple(Command(this.id, this.cmds['get_control_parameters']), 'ffII',
                               (tup: [number, number, number, number]) => {
            this.status.fs = tup[0];
            this.status.tx_duration = tup[1];
            this.status.cic_rate = tup[2];
            this.status.n_pts = tup[3];

            cb(this.status);
        });
    }
}
