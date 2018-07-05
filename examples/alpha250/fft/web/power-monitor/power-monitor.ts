class PowerMonitor {
    private driver: Driver;
    private id: number;
    private cmds: HashTable<ICommand>;

    constructor (private client: Client) {
        this.driver = this.client.getDriver('PowerMonitor');
        this.id = this.driver.id;
        this.cmds = this.driver.getCmds();
    }

    getSuppliesUI(cb: (values: Float32Array) => void): void {
        this.client.readFloat32Array(Command(this.id, this.cmds['get_supplies_ui']),
                                 (values: Float32Array) => {cb(values)});
    }
}