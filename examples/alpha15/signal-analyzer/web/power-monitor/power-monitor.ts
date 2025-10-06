class PowerMonitor {
    private driver: Driver;
    private id: number;
    private cmds: HashTable<ICommand>;

    constructor (private client: Client) {
        this.driver = this.client.getDriver('PowerMonitor');
        this.id = this.driver.id;
        this.cmds = this.driver.getCmds();
    }

    async getSuppliesUI(): Promise<Float32Array> {
        return await this.client.readFloat32Array(Command(this.id, this.cmds['get_supplies_ui']));
    }
}