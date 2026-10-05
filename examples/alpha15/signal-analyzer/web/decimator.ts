// Interface for the Decimator driver
// (c) Koheron

interface IDecimatorStatus {
    fs: number;          // Sampling frequency (Hz)
    fs_lf: number;          // Sampling frequency (Hz)
    tx_duration: number; // FIFO transfer duration (s)
    tx_duration_lf: number; // FIFO transfer duration (s)
    cic_rate: number;
    cic_rate_lf: number;
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

    async init(): Promise<void> {
        await this.getControlParameters();
    }

    setFFTWindow(windowIndex: number): void {
        this.client.send(Command(this.id, this.cmds['set_fft_window'], windowIndex));
    }

    restartAcquisition(): Promise<number> {
        return this.client.readUint32(Command(this.id, this.cmds['restart_acquisition']));
    }

    readSnapshot(band: number): Promise<{metadata: number[]; values: Float32Array}> {
        return this.client.readTupleWithFloat32Vector(Command(this.id,
            this.cmds[band === 0 ? 'get_spectrum_snapshot1' : 'get_spectrum_snapshot0']), 'IQ', 12);
    }

    async spectralDensity(): Promise<Float64Array> {
        return await this.client.readFloat64Vector(Command(this.id, this.cmds['spectral_density0']));
    }

    async spectralDensityLf(): Promise<Float64Array> {
        return await this.client.readFloat64Vector(Command(this.id, this.cmds['spectral_density1']));
    }

    async getControlParameters(): Promise<IDecimatorStatus> {
        const tuple = await this.client.readTuple(Command(this.id, this.cmds['get_control_parameters']), 'ffffIII');
        this.status = {fs: tuple[0], fs_lf: tuple[1], tx_duration: tuple[2], tx_duration_lf: tuple[3],
            cic_rate: tuple[4], cic_rate_lf: tuple[5], n_pts: tuple[6]};
        return this.status;
    }
}
