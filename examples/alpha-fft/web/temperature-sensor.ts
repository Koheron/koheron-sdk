class TemperatureSensor {
    private driver: Driver;
    private id: number;
    private cmds: HashTable<ICommand>;

    constructor (private client: Client) {
        this.driver = this.client.getDriver('TemperatureSensor');
        this.id = this.driver.id;
        this.cmds = this.driver.getCmds();
    }

    getTemperature(idx: number, cb: (value: number) => void): void {
        this.client.readFloat32(Command(this.id, this.cmds['get_temperature'], idx),
                                 (value) => {cb(value)});
    }
}