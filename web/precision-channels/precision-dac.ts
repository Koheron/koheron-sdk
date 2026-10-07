class PrecisionDac {
    private driver: Driver;
    private id: number;
    private cmds: HashTable<ICommand>;

    constructor (private client: Client) {
        this.driver = this.client.getDriver('PrecisionDac');
        this.id = this.driver.id;
        this.cmds = this.driver.getCmds();
    }

    setDac(channel: number, voltage: number): void {
        this.client.send(Command(this.id, this.cmds['set_dac_value_volts'], channel, voltage));
    }

    getDacValues(cb: (values: Float32Array) => void): void;
    getDacValues(): Promise<Float32Array>;
    getDacValues(cb?: (values: Float32Array) => void): void | Promise<Float32Array> {
        if (cb) {
            this.client.readFloat32Array(Command(this.id, this.cmds['get_dac_values']),
                (values: Float32Array) => {cb(values)});
        } else {
            return this.client.readFloat32Array(Command(this.id, this.cmds['get_dac_values']));
        }
    }
}
