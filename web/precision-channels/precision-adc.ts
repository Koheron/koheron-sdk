class PrecisionAdc {
    private driver: Driver;
    private id: number;
    private cmds: HashTable<ICommand>;

    constructor (private client: Client) {
        this.driver = this.client.getDriver('PrecisionAdc');
        this.id = this.driver.id;
        this.cmds = this.driver.getCmds();
    }

    getAdcValues(): Promise<Float32Array> {
        return this.client.readFloat32Array(Command(this.id, this.cmds['get_adc_values']));
    }
}
