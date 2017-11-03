class PowerMonitor {
    private driver: Driver;
    private id: number;
    private cmds: HashTable<ICommand>;

    constructor (private client: Client) {
        this.driver = this.client.getDriver('PowerMonitor');
        this.id = this.driver.id;
        this.cmds = this.driver.getCmds();
    }

    getSupplyCurrent(idx: number, cb: (value: number) => void): void {
        this.client.readFloat32(Command(this.id, this.cmds['get_shunt_voltage'], idx),
                                 (value) => {cb(value/0.01)}); // 10 mOhm shunt
    }

    getBusVoltage(idx: number, cb: (value: number) => void): void {
        this.client.readFloat32(Command(this.id, this.cmds['get_bus_voltage'], idx),
                                 (value) => {cb(value)});
    }
}