class PowerMonitor {
    private driver: Driver;
    private id: number;
    private cmds: HashTable<ICommand>;

    constructor (private client: Client) {
        this.driver = this.client.getDriver('PowerMonitor');
        this.id = this.driver.id;
        this.cmds = this.driver.getCmds();
    }

    getSuppliesUI(cb: (values: Float32Array) => void): void;
    getSuppliesUI(): Promise<Float32Array>;
    getSuppliesUI(cb?: (values: Float32Array) => void): void | Promise<Float32Array> {
        const command = Command(this.id, this.cmds['get_supplies_ui']);
        if (cb) {
            this.client.readFloat32Array(command, cb);
        } else {
            return this.client.readFloat32Array(command);
        }
    }
}
