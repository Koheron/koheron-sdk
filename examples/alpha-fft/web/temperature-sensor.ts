class TemperatureSensor {
    private driver: Driver;
    private id: number;
    private cmds: HashTable<ICommand>;

    constructor (private client: Client) {
        this.driver = this.client.getDriver('TemperatureSensor');
        this.id = this.driver.id;
        this.cmds = this.driver.getCmds();
    }

    getTemperatures(cb: (temperatures: Float32Array) => void): void {
        this.client.readFloat32Array(Command(this.id, this.cmds['get_temperatures']),
                                 (temperatures: Float32Array) => {cb(temperatures)});
    }
}