class Monitor {
    private driver: Driver;
    private id: number;
    private cmds: HashTable<ICommand>;

    constructor (private client: Client) {
        this.driver = this.client.getDriver('Monitor');
        this.id = this.driver.id;
        this.cmds = this.driver.getCmds();
    }

    getTemperature(cb: (value: number) => void): void {
        this.client.readFloat32(Command(this.id, this.cmds['get_temperature']), (value) => {cb(value)});
    }

}