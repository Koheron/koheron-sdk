// Interface for PulseGenerator driver
// (c) Koheron

class PulseGenerator {
    private driver: Driver;
    private id: number;
    private cmds: HashTable<ICommand>;

    constructor (private client: Client) {
        this.driver = this.client.getDriver('Pulse');
        this.id = this.driver.id;
        this.cmds = this.driver.getCmds();
    }

    trigPulse(): void {
        this.client.send(Command(this.id, this.cmds['trig_pulse']));
    }

    setPulseGenerator(pulse_width: number, pulse_period: number) {
        this.client.send(Command(this.id, this.cmds['set_pulse_generator'], pulse_width, pulse_period));
    }

    setDacData(data: Uint32Array): void {
        this.client.send(Command(this.id, this.cmds['set_dac_data'], data));
    }

    getNextPulse(n_pts: number, cb: (adc_data: Uint32Array) => void): void {
        this.client.readUint32Vector(Command(this.id, this.cmds['get_next_pulse'], n_pts), (adc_data: Uint32Array) => {
            cb(adc_data);
        });
    }

}