// Interface for PulseGenerator driver
// (c) Koheron

interface PulseStatus {
    width: number;
    period: number;
}


class PulseGenerator {
    private driver: Driver;
    private id: number;
    private cmds: HashTable<ICommand>;

    private adc0: number[][]; // Volts
    private adc1: number[][]; // Volts

    private n_pts: number = 1000;

    constructor (private client: Client) {
        this.driver = this.client.getDriver('Pulse');
        this.id = this.driver.id;
        this.cmds = this.driver.getCmds();

        this.adc0 = new Array(this.n_pts);
        this.adc1 = new Array(this.n_pts);

        for (var i: number = 0, len: number = this.adc0.length; i < len; i++ ) {
            this.adc0[i] = [i, 0];
            this.adc1[i] = [i, 0];
        }
    }

    trigPulse(): void {
        this.client.send(Command(this.id, this.cmds['trig_pulse']));
    }

    getStatus(cb: (status: PulseStatus) => void): void {
        this.client.readTuple(Command(this.id, this.cmds['get_status']), 'II', (tup: number[]) => {
            let status: PulseStatus = <PulseStatus>{};
            status.width = tup[0];
            status.period = tup[1];
            cb(status);
        });
    }

    setPulseWidth(width: number) {
        this.client.send(Command(this.id, this.cmds['set_pulse_width'], width));
    }

    setPulsePeriod(period: number) {
        this.client.send(Command(this.id, this.cmds['set_pulse_period'], period));
    }

    setDacData(data: Uint32Array): void {
        this.client.send(Command(this.id, this.cmds['set_dac_data'], data));
    }

    getCount(cb: (i: number) => void): void {
        this.client.readUint32(Command(this.id, this.cmds['get_count']), (i) => {cb(i)});
    }

    getNextPulse(n_pts: number, cb: (adc_data: Uint32Array) => void): void {
        this.client.readUint32Vector(Command(this.id, this.cmds['get_next_pulse'], n_pts), (adc_data: Uint32Array) => {
            cb(adc_data);
        });
    }

    getFifoBuffer(cb: (adc0: Array<Array<number>>, adc1: Array<Array<number>>) => void): void {
        this.client.readUint32Array(Command(this.id, this.cmds['get_fifo_buffer']), (data_rcv: Uint32Array) => {
            for (var i: number = 0, len: number = data_rcv.length; i < len; i++) {
                this.adc0[i] = [i, (((data_rcv[i] % 16384) + 8192) % 16384 - 8192) / 8192];
                this.adc1[i] = [i, ((((data_rcv[i] >>> 16) % 16384) + 8192) % 16384 - 8192) / 8192];
            }

            cb(this.adc0, this.adc1);
        });
    }

}