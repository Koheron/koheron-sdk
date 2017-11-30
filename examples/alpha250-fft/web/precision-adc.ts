class PrecisionAdc {
    private driver: Driver;
    private id: number;
    private cmds: HashTable<ICommand>;

    constructor (private client: Client) {
        this.driver = this.client.getDriver('PrecisionAdc');
        this.id = this.driver.id;
        this.cmds = this.driver.getCmds();
    }

    getAdcValues(cb: (inputs: Float32Array) => void): void {
        this.client.readFloat32Array(Command(this.id, this.cmds['get_adc_values']),
                                 (inputs: Float32Array) => {cb(inputs)});
    }
}