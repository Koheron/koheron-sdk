// Interface for the DDS driver
// (c) Koheron

interface IFFTStatus {
    dds_freq: number[];
}

class FFT {
    private driver: Driver;
    private id: number;
    private cmds: HashTable<ICommand>;

    constructor (private client: Client) {
        this.driver = this.client.getDriver('FFT');
        this.id = this.driver.id;
        this.cmds = this.driver.getCmds();
        //this.monitor(1000);
    }

    monitor(timeout: number): void {
        this.getCycleIndex( (i) => {
            console.log('Cycle index ' + i.toString());
            setTimeout( () => {
                this.monitor(timeout);
            }, timeout);
        });
    }

    getCycleIndex(cb: (i: number) => void): void {
        this.client.readUint32(Command(this.id, this.cmds['get_cycle_index']),
                                 (i) => {cb(i)});
    }

    read_psd(cb: (psd: Float32Array) => void): void {
        this.client.readFloat32Array(Command(this.id, this.cmds['read_psd']), (psd: Float32Array) => {
            cb(psd);
        });
    }

    setDDSFreq(channel: number, freq_hz: number): void {
        this.client.send(Command(this.id, this.cmds['set_dds_freq'], channel, freq_hz));
    }

    // selectPsdInput(psd_input_sel: number): void {
    //     this.client.send(Command(this.id, this.cmds['select_psd_input'], psd_input_sel));
    // }

    getControlParameters(cb: (status: IFFTStatus) => void): void {
        this.client.readTuple(Command(this.id, this.cmds['get_control_parameters']), 'dd',
                               (tup: [number, number, number]) => {
            let status: IFFTStatus = <IFFTStatus>{};
            status.dds_freq = [];
            status.dds_freq[0] = tup[0];
            status.dds_freq[1] = tup[1];
            cb(status);
        });
    }
}
