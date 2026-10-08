class TemperatureSensor {
    private driver: Driver;
    private id: number;
    private cmds: HashTable<ICommand>;

    constructor (private client: Client) {
        this.driver = this.client.getDriver('TemperatureSensor');
        this.id = this.driver.id;
        this.cmds = this.driver.getCmds();
    }

    getTemperatures(cb: (values: Float32Array) => void): void;
    getTemperatures(): Promise<Float32Array>;
    getTemperatures(cb?: (values: Float32Array) => void): void | Promise<Float32Array> {
        const command = Command(this.id, this.cmds['get_temperatures']);
        if (cb) {
            this.client.readFloat32Array(command, cb);
        } else {
            return this.client.readFloat32Array(command);
        }
    }
}
